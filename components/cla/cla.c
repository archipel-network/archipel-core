// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"

#include "cla/posix/cla_mtcp.h"
#include "cla/posix/cla_smtcp.h"
#include "cla/posix/cla_tcpclv3.h"
#include "cla/posix/cla_tcpspp.h"
#include "cla/posix/cla_bibe.h"

#include "platform/hal_io.h"
#include "platform/hal_task.h"
#include "platform/hal_time.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"

#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/init.h"
#include "ud3tn/result.h"

#include <unistd.h>

#include <stdbool.h>
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
	{ "mtcp", &mtcp_create },
	{ "smtcp", &smtcp_create },
	{ "tcpclv3", &tcpclv3_create },
	{ "tcpspp", &tcpspp_create },
	{ "bibe", &bibe_create },
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
				struct cla_config *config,
				char *const cla_addr,
				const bool is_rx, const bool is_tx)
{
	link->config = config;

	link->cla_addr = cla_addr ? strdup(cla_addr) : NULL;

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

	link->rx_task_notification = hal_semaphore_init_binary();
	if (!link->rx_task_notification) {
		LOG("CLA: Cannot allocate memory for RX notify semaphore!");
		goto fail_rx_notify_sem;
	}
	hal_semaphore_release(link->rx_task_notification);

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

	if (is_rx) {
		if (cla_launch_contact_rx_task(link) != UD3TN_OK) {
			LOG("CLA: Failed to start RX task!");
			goto fail_rx_task;
		}
	}

	if (is_tx) {
		if (cla_launch_contact_tx_task(link) != UD3TN_OK) {
			LOG("CLA: Failed to start TX task!");
			// The RX task already takes care of the link; we MUST
			// NOT invalidate the associated data. The TX task
			// semaphore is not locked, so it is properly treated
			// as not being running.
			// We also report UD3TN_OK as otherwise the caller will
			// consider `link` invalid, which it is not in this
			// case. However, we do not trigger BP and inform the
			// RX task that it should terminate.
			hal_semaphore_try_take(link->rx_task_notification, 0);
			return UD3TN_OK;
		}

		// Notify the BP task of the newly established connection...
		const struct bundle_agent_interface *bundle_agent_interface =
			config->bundle_agent_interface;
		bundle_processor_inform(
			bundle_agent_interface->bundle_signaling_queue,
			NULL,
			BP_SIGNAL_NEW_LINK_ESTABLISHED,
			cla_get_cla_addr_from_link(link),
			NULL,
			NULL,
			NULL
		);
	}

	return UD3TN_OK;

fail_rx_task:
	hal_semaphore_delete(link->tx_queue_sem);
fail_tx_queue_sem:
	hal_queue_delete(link->tx_queue_handle);
fail_tx_queue:
	rx_task_data_deinit(&link->rx_task_data);
fail_rx_data:
	hal_semaphore_delete(link->rx_task_notification);
fail_rx_notify_sem:
	hal_semaphore_delete(link->tx_task_sem);
fail_tx_sem:
	hal_semaphore_delete(link->rx_task_sem);
fail_rx_sem:
	return UD3TN_FAIL;
}

void cla_link_wait_cleanup(struct cla_link *link)
{
	cla_link_wait(link);
	cla_link_cleanup(link);
}

void cla_link_wait(struct cla_link *link)
{
	// Wait for graceful termination of tasks
	hal_semaphore_take_blocking(link->rx_task_sem);
	hal_semaphore_take_blocking(link->tx_task_sem);
}

void cla_link_cleanup(struct cla_link *link)
{
	// Clean up semaphores
	hal_semaphore_delete(link->rx_task_sem);
	hal_semaphore_delete(link->tx_task_sem);
	hal_semaphore_delete(link->rx_task_notification);

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

	free(link->cla_addr);
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
	hal_semaphore_try_take(link->rx_task_notification, 0);
	// Notify dispatcher that the connection was lost
	const struct bundle_agent_interface *bundle_agent_interface =
		link->config->bundle_agent_interface;
	bundle_processor_inform(
		bundle_agent_interface->bundle_signaling_queue,
		NULL,
		BP_SIGNAL_LINK_DOWN,
		cla_get_cla_addr_from_link(link),
		NULL,
		NULL,
		NULL
	);
	// TX task will delete its queue and itself
	cla_contact_tx_task_request_exit(link->tx_queue_handle);
	// The termination of the tasks means cla_link_wait_cleanup returns
}

char *cla_get_cla_addr_from_link(const struct cla_link *const link)
{
	const char *const cla_name = link->config->vtable->cla_name_get();

	if (!cla_name)
		return NULL;

	const size_t cla_name_len = strlen(cla_name);

	const char *const addr = link->cla_addr;
	const size_t addr_len = addr ? strlen(addr) : 0;

	// <cla_name>:<addr>\0
	const size_t result_len = cla_name_len + 1 + addr_len + 1;
	char *const result = malloc(result_len);

	ASSERT(
		snprintf(result, result_len, "%s", cla_name) ==
		(int64_t)cla_name_len
	);
	result[cla_name_len] = ':';
	if (addr)
		ASSERT(snprintf(
			result + cla_name_len + 1,
			result_len - 1 - cla_name_len,
			"%s",
			addr
		) == (int64_t)addr_len);
	result[result_len - 1] = '\0';

	return result;
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
	if (!cla_addr)
		return NULL;

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
