#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"

#ifndef PLATFORM_STM32
#include "cla/posix/cla_mtcp.h"
#include "cla/posix/cla_smtcp.h"
#include "cla/posix/cla_tcpclv3.h"
#include "cla/posix/cla_tcpspp.h"
#else // PLATFORM_STM32
#include "cla/stm32/cla_usbotg.h"
#endif // PLATFORM_STM32

#include "platform/hal_io.h"
#include "platform/hal_task.h"
#include "platform/hal_time.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"

#include "ud3tn/common.h"
#include "ud3tn/init.h"
#include "ud3tn/result.h"
#include "ud3tn/router_task.h"

#include <unistd.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct available_cla_list_entry {
	const char *name;
	struct cla_config *(*create_func)(
		const char *const *options,
		size_t options_count,
		const struct bundle_agent_interface *bundle_agent_interface);
};

const struct available_cla_list_entry AVAILABLE_CLAS[] = {
#ifndef PLATFORM_STM32
	{ "mtcp", &mtcp_create },
	{ "smtcp", &smtcp_create },
	{ "tcpclv3", &tcpclv3_create },
	{ "tcpspp", &tcpspp_create },
#else // PLATFORM_STM32
	{ "usbotg", &usbotg_create },
#endif // PLATFORM_STM32
};


static void cla_register(struct cla_config *config);

static enum ud3tn_result initialize_single(
	char *cur_cla_config,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	// NOTE that currently we only support the format <name>:<option>,...
	char *colon = strchr(cur_cla_config, ':');

	ASSERT(cur_cla_config);
	if (!colon) {
		LOG("CLA: Could parse config - options delimiter not found!");
		return UD3TN_FAIL;
	}

	// Null-terminate the CLA name
	colon[0] = 0;

	// Split the rest of the options
	size_t options_count = 1;
	char *next_delim = &colon[1];
	const char *options_array[CLA_MAX_OPTION_COUNT];

	options_array[0] = &colon[1];
	while (options_count < CLA_MAX_OPTION_COUNT &&
			(next_delim = strchr(next_delim, ','))) {
		next_delim[0] = 0;
		next_delim++;
		options_array[options_count] = next_delim;
		options_count++;
	}

	const char *cla_name = cur_cla_config;
	const struct available_cla_list_entry *cla_entry = NULL;

	for (size_t i = 0; i < ARRAY_LENGTH(AVAILABLE_CLAS); i++) {
		if (strcmp(AVAILABLE_CLAS[i].name, cla_name) == 0) {
			cla_entry = &AVAILABLE_CLAS[i];
			break;
		}
	}
	if (!cla_entry) {
		LOGF("CLA: Specified CLA not found: %s", cla_name);
		return UD3TN_FAIL;
	}

	struct cla_config *data = cla_entry->create_func(
		options_array,
		options_count,
		bundle_agent_interface
	);

	if (!data) {
		LOGF("CLA: Could not initialize CLA \"%s\"!", cla_name);
		return UD3TN_FAIL;
	}

	data->vtable->cla_launch(data);
	cla_register(data);
	LOGF("CLA: Activated CLA \"%s\".", data->vtable->cla_name_get());

	return UD3TN_OK;
}

enum ud3tn_result cla_initialize_all(
	const char *cla_config_str,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (!cla_config_str)
		return UD3TN_FAIL;

	char *const cla_config_str_dup = strdup(cla_config_str);
	char *cur_cla_config = cla_config_str_dup;
	char *comma = strchr(cur_cla_config, ';');
	enum ud3tn_result result = UD3TN_FAIL;

	while (comma) {
		// Null-terminate the current part of the string
		comma[0] = 0;
		// End of string encountered
		if (comma[1] == 0)
			break;
		if (initialize_single(cur_cla_config,
				      bundle_agent_interface) != UD3TN_OK)
			goto cleanup;
		cur_cla_config = &comma[1];
		comma = strchr(cur_cla_config, ';');
	}
	result = initialize_single(cur_cla_config, bundle_agent_interface);

cleanup:
	free(cla_config_str_dup);
	return result;
}

enum ud3tn_result cla_config_init(
	struct cla_config *config,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	config->vtable = NULL;
	config->bundle_agent_interface = bundle_agent_interface;

	return UD3TN_OK;
}

enum ud3tn_result cla_link_init(struct cla_link *link,
				struct cla_config *config)
{
	link->config = config;
	link->active = true;

	link->rx_task_handle = NULL;
	link->tx_task_handle = NULL;

	link->last_rx_time_ms = hal_time_get_timestamp_ms();

	link->tx_queue_handle = NULL;
	link->tx_queue_sem = NULL;

	// Semaphores used for waiting for the tasks to exit
	// NOTE: They are already locked on creation!
	link->rx_task_sem = hal_semaphore_init_binary();
	if (!link->rx_task_sem) {
		LOG("CLA: Cannot allocate memory for RX semaphore!");
		goto fail_rx_sem;
	}
	hal_semaphore_release(link->rx_task_sem);
	link->tx_task_sem = hal_semaphore_init_binary();
	if (!link->tx_task_sem) {
		LOG("CLA: Cannot allocate memory for TX semaphore!");
		goto fail_tx_sem;
	}
	hal_semaphore_release(link->tx_task_sem);

	if (rx_task_data_init(&link->rx_task_data, config) != UD3TN_OK) {
		LOG("CLA: Failed to initialize RX task data!");
		goto fail_rx_data;
	}
	config->vtable->cla_rx_task_reset_parsers(link);

	link->tx_queue_handle = hal_queue_create(
		CONTACT_TX_TASK_QUEUE_LENGTH,
		sizeof(struct cla_contact_tx_task_command)
	);
	if (link->tx_queue_handle == NULL)
		goto fail_tx_queue;

	link->tx_queue_sem = hal_semaphore_init_binary();
	if (!link->tx_queue_sem) {
		LOG("CLA: Cannot allocate memory for TX queue semaphore!");
		goto fail_tx_queue_sem;
	}
	hal_semaphore_release(link->tx_queue_sem);

	if (cla_launch_contact_rx_task(link) != UD3TN_OK) {
		LOG("CLA: Failed to start RX task!");
		goto fail_rx_task;
	}

	if (cla_launch_contact_tx_task(link) != UD3TN_OK) {
		LOG("CLA: Failed to start TX task!");
		goto fail_tx_task;
	}

	// Notify the router task of the newly established connection...
	struct router_signal rt_signal = {
		.type = ROUTER_SIGNAL_NEW_LINK_ESTABLISHED,
		.data = NULL,
	};
	const struct bundle_agent_interface *const bundle_agent_interface =
		config->bundle_agent_interface;
	hal_queue_push_to_back(bundle_agent_interface->router_signaling_queue,
			       &rt_signal);

	return UD3TN_OK;

fail_tx_task:
	hal_task_delete(link->rx_task_handle);
fail_rx_task:
	hal_semaphore_delete(link->tx_queue_sem);
fail_tx_queue_sem:
	hal_queue_delete(link->tx_queue_handle);
fail_tx_queue:
	rx_task_data_deinit(&link->rx_task_data);
fail_rx_data:
	hal_semaphore_delete(link->tx_task_sem);
fail_tx_sem:
	hal_semaphore_delete(link->rx_task_sem);
fail_rx_sem:
	return UD3TN_FAIL;
}

void cla_link_wait_cleanup(struct cla_link *link)
{
	// Wait for graceful termination of tasks
	hal_semaphore_take_blocking(link->rx_task_sem);
	hal_semaphore_take_blocking(link->tx_task_sem);

	// Clean up semaphores
	hal_semaphore_delete(link->rx_task_sem);
	hal_semaphore_delete(link->tx_task_sem);

	// The TX task ensures the queue is locked and empty before terminating
	QueueIdentifier_t tx_queue_handle = link->tx_queue_handle;

	// Invalidate queue and unblock anyone waiting to put sth. in the queue
	link->tx_queue_handle = NULL;
	while (hal_semaphore_try_take(link->tx_queue_sem, 0) != UD3TN_OK)
		hal_semaphore_release(link->tx_queue_sem);

	// Finally drop the tx semaphore and queue handle
	hal_semaphore_delete(link->tx_queue_sem);
	hal_queue_delete(tx_queue_handle);

	link->config->vtable->cla_rx_task_reset_parsers(link);
	rx_task_data_deinit(&link->rx_task_data);
}

char *cla_get_connect_addr(const char *cla_addr, const char *cla_name)
{
	const char *offset = strchr(cla_addr, ':');

	if (!offset)
		return NULL;
	ASSERT(offset - cla_addr == (ssize_t)strlen(cla_name));
	ASSERT(memcmp(cla_addr, cla_name, offset - cla_addr) == 0);
	return strdup(offset + 1);
}

void cla_generic_disconnect_handler(struct cla_link *link)
{
	// RX task will delete itself
	link->active = false;
	// TX task will delete its queue and itself
	cla_contact_tx_task_request_exit(link->tx_queue_handle);
	// The termination of the tasks means cla_link_wait_cleanup returns
}

// CLA Instance Management

static struct cla_config *global_instances[ARRAY_SIZE(AVAILABLE_CLAS)];

static void cla_register(struct cla_config *config)
{
	const char *name = config->vtable->cla_name_get();

	for (size_t i = 0; i < ARRAY_SIZE(AVAILABLE_CLAS); i++) {
		if (strcmp(AVAILABLE_CLAS[i].name, name) == 0) {
			global_instances[i] = config;
			return;
		}
	}
	LOGF("CLA: FATAL: Could not globally register CLA \"%s\"", name);
	ASSERT(0);
}

struct cla_config *cla_config_get(const char *cla_addr)
{
	ASSERT(cla_addr != NULL);
	const size_t addr_len = strlen(cla_addr);

	for (size_t i = 0; i < ARRAY_SIZE(AVAILABLE_CLAS); i++) {
		const char *name = AVAILABLE_CLAS[i].name;
		const size_t name_len = strlen(name);

		if (addr_len < name_len)
			continue;
		if (memcmp(name, cla_addr, name_len) == 0) {
			if (!global_instances[i])
				LOGF("CLA \"%s\" compiled-in but not enabled!",
				     name);
			return global_instances[i];
		}
	}
	LOGF("CLA: Could not determine instance for addr.: \"%s\"", cla_addr);
	return NULL;
}
