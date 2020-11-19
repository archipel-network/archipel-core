#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"
#include "cla/mtcp_proto.h"
#include "cla/posix/cla_mtcp.h"
#include "cla/posix/cla_tcp_common.h"
#include "cla/posix/cla_tcp_util.h"

#include "bundle6/parser.h"
#include "bundle7/parser.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"
#include "platform/hal_types.h"

#include "ud3tn/bundle_agent_interface.h"
#include "ud3tn/cmdline.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/result.h"
#include "ud3tn/simplehtab.h"
#include "ud3tn/task_tags.h"

#include <sys/socket.h>
#include <unistd.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct mtcp_config {
	struct cla_tcp_config base;

	struct htab_entrylist *param_htab_elem[CLA_TCP_PARAM_HTAB_SLOT_COUNT];
	struct htab param_htab;
	Semaphore_t param_htab_sem;
};

struct mtcp_contact_parameters {
	// IMPORTANT: The cla_tcp_link is only initialized iff connected == true
	struct mtcp_link link;

	struct mtcp_config *config;

	Task_t management_task;

	char *cla_sock_addr;

	bool in_contact;
	bool connected;
	int connect_attempt;

	int socket;
};


static enum ud3tn_result handle_established_connection(
	struct mtcp_contact_parameters *const param)
{
	struct mtcp_config *const mtcp_config = param->config;

	if (cla_tcp_link_init(&param->link.base, param->socket,
			      &mtcp_config->base)
			!= UD3TN_OK) {
		LOG("MTCP: Error initializing CLA link!");
		return UD3TN_FAIL;
	}

	cla_link_wait_cleanup(&param->link.base.base);

	return UD3TN_OK;
}

static void mtcp_link_management_task(void *p)
{
	struct mtcp_contact_parameters *const param = p;

	ASSERT(param->cla_sock_addr != NULL);
	do {
		if (param->connected) {
			ASSERT(param->socket > 0);
			handle_established_connection(param);
			param->connected = false;
			param->connect_attempt = 0;
			param->socket = -1;
		} else {
			ASSERT(param->socket < 0);
			param->socket = cla_tcp_connect_to_cla_addr(
				param->cla_sock_addr,
				NULL
			);
			if (param->socket < 0) {
				if (++param->connect_attempt >
						CLA_TCP_MAX_RETRY_ATTEMPTS) {
					LOG("MTCP: Final retry failed.");
					break;
				}
				LOGF("MTCP: Delayed retry %d of %d in %d ms",
				     param->connect_attempt,
				     CLA_TCP_MAX_RETRY_ATTEMPTS,
				     CLA_TCP_RETRY_INTERVAL_MS);
				hal_task_delay(CLA_TCP_RETRY_INTERVAL_MS);
				continue;
			}
			LOGF("MTCP: Connected successfully to \"%s\"",
			     param->cla_sock_addr);
			param->connected = true;
		}
	} while (param->in_contact);
	LOGF("MTCP: Terminating contact link manager for \"%s\"",
	     param->cla_sock_addr);
	hal_semaphore_take_blocking(param->config->param_htab_sem);
	htab_remove(&param->config->param_htab, param->cla_sock_addr);
	hal_semaphore_release(param->config->param_htab_sem);
	mtcp_parser_reset(&param->link.mtcp_parser);
	free(param->cla_sock_addr);

	Task_t management_task = param->management_task;

	free(param);
	hal_task_delete(management_task);
}

static void launch_connection_management_task(
	struct mtcp_config *const mtcp_config,
	const int sock, const char *cla_addr)
{
	ASSERT(cla_addr);
	struct mtcp_contact_parameters *contact_params =
		malloc(sizeof(struct mtcp_contact_parameters));

	if (!contact_params) {
		LOG("MTCP: Failed to allocate memory!");
		return;
	}

	contact_params->config = mtcp_config;
	contact_params->connect_attempt = 0;

	if (sock < 0) {
		contact_params->cla_sock_addr = cla_get_connect_addr(
			cla_addr,
			"mtcp"
		);
		contact_params->socket = -1;
		contact_params->connected = false;
		contact_params->in_contact = true;
	} else {
		ASSERT(sock != -1);
		contact_params->cla_sock_addr = strdup(cla_addr);
		contact_params->socket = sock;
		contact_params->connected = true;
		contact_params->in_contact = false;
	}

	if (!contact_params->cla_sock_addr) {
		LOG("MTCP: Failed to copy CLA address!");
		goto fail;
	}

	mtcp_parser_reset(&contact_params->link.mtcp_parser);

	struct htab_entrylist *htab_entry = NULL;

	htab_entry = htab_add(
		&mtcp_config->param_htab,
		contact_params->cla_sock_addr,
		contact_params
	);
	if (!htab_entry) {
		LOG("MTCP: Error creating htab entry!");
		goto fail;
	}

	contact_params->management_task = hal_task_create(
		mtcp_link_management_task,
		"mtcp_mgmt_t",
		CONTACT_MANAGEMENT_TASK_PRIORITY,
		contact_params,
		CONTACT_MANAGEMENT_TASK_STACK_SIZE,
		(void *)CLA_SPECIFIC_TASK_TAG
	);

	if (!contact_params->management_task) {
		LOG("MTCP: Error creating management task!");
		if (htab_entry) {
			ASSERT(contact_params->cla_sock_addr);
			ASSERT(htab_remove(
				&mtcp_config->param_htab,
				contact_params->cla_sock_addr
			) == contact_params);
		}
		goto fail;
	}

	return;

fail:
	free(contact_params->cla_sock_addr);
	free(contact_params);
}

static void mtcp_listener_task(void *param)
{
	struct mtcp_config *const mtcp_config = param;
	char *cla_addr;
	int sock;

	for (;;) {
		sock = cla_tcp_accept_from_socket(
			&mtcp_config->base,
			mtcp_config->base.socket,
			&cla_addr
		);
		if (sock == -1)
			break;

		hal_semaphore_take_blocking(mtcp_config->param_htab_sem);
		launch_connection_management_task(
			mtcp_config,
			sock,
			cla_addr
		);
		hal_semaphore_release(mtcp_config->param_htab_sem);
		free(cla_addr);
	}
	// unexpected failure to accept() - exit thread in release mode
	ASSERT(0);
}

static enum ud3tn_result mtcp_launch(struct cla_config *const config)
{
	struct mtcp_config *const mtcp_config = (struct mtcp_config *)config;

	mtcp_config->base.listen_task = hal_task_create(
		mtcp_listener_task,
		"mtcp_listen_t",
		CONTACT_LISTEN_TASK_PRIORITY,
		config,
		CONTACT_LISTEN_TASK_STACK_SIZE,
		(void *)CLA_SPECIFIC_TASK_TAG
	);

	if (!mtcp_config->base.listen_task)
		return UD3TN_FAIL;

	return UD3TN_OK;
}

static const char *mtcp_name_get(void)
{
	return "mtcp";
}

size_t mtcp_mbs_get(struct cla_config *const config)
{
	(void)config;
	return SIZE_MAX;
}

void mtcp_reset_parsers(struct cla_link *link)
{
	struct mtcp_link *const mtcp_link = (struct mtcp_link *)link;

	rx_task_reset_parsers(&link->rx_task_data);

	mtcp_parser_reset(&mtcp_link->mtcp_parser);
	link->rx_task_data.cur_parser = &mtcp_link->mtcp_parser;
}

size_t mtcp_forward_to_specific_parser(struct cla_link *link,
				       const uint8_t *buffer, size_t length)
{
	struct mtcp_link *const mtcp_link = (struct mtcp_link *)link;
	struct rx_task_data *const rx_data = &link->rx_task_data;
	size_t result = 0;

	// Decode MTCP CBOR byte string header if not done already
	if (!(mtcp_link->mtcp_parser.flags & PARSER_FLAG_DATA_SUBPARSER))
		return mtcp_parser_parse(&mtcp_link->mtcp_parser,
					 buffer, length);

	// We do not allow to parse more than the stated length...
	if (length > mtcp_link->mtcp_parser.next_bytes)
		length = mtcp_link->mtcp_parser.next_bytes;

	switch (rx_data->payload_type) {
	case PAYLOAD_UNKNOWN:
		result = select_bundle_parser_version(rx_data, buffer, length);
		if (result == 0)
			mtcp_reset_parsers(link);
		break;
	case PAYLOAD_BUNDLE6:
		rx_data->cur_parser = rx_data->bundle6_parser.basedata;
		result = bundle6_parser_read(
			&rx_data->bundle6_parser,
			buffer,
			length
		);
		break;
	case PAYLOAD_BUNDLE7:
		rx_data->cur_parser = rx_data->bundle7_parser.basedata;
		result = bundle7_parser_read(
			&rx_data->bundle7_parser,
			buffer,
			length
		);
		break;
	default:
		mtcp_reset_parsers(link);
		return 0;
	}

	ASSERT(result <= mtcp_link->mtcp_parser.next_bytes);
	mtcp_link->mtcp_parser.next_bytes -= result;

	// All done
	if (!mtcp_link->mtcp_parser.next_bytes)
		mtcp_reset_parsers(link);

	return result;
}

/*
 * TX
 */

static struct mtcp_contact_parameters *get_contact_parameters(
	struct cla_config *config, const char *cla_addr)
{
	struct mtcp_config *const mtcp_config =
		(struct mtcp_config *)config;
	char *const cla_sock_addr = cla_get_connect_addr(cla_addr, "mtcp");

	struct mtcp_contact_parameters *param = htab_get(
		&mtcp_config->param_htab,
		cla_sock_addr
	);
	free(cla_sock_addr);
	return param;
}

static struct cla_tx_queue mtcp_get_tx_queue(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	(void)eid;
	struct mtcp_config *const mtcp_config = (struct mtcp_config *)config;

	hal_semaphore_take_blocking(mtcp_config->param_htab_sem);
	struct mtcp_contact_parameters *const param = get_contact_parameters(
		config,
		cla_addr
	);

	if (param && param->connected) {
		struct cla_link *const cla_link = &param->link.base.base;

		hal_semaphore_take_blocking(cla_link->tx_queue_sem);
		hal_semaphore_release(mtcp_config->param_htab_sem);

		// Freed while trying to obtain it
		if (!cla_link->tx_queue_handle)
			return (struct cla_tx_queue){ NULL, NULL };

		return (struct cla_tx_queue){
			.tx_queue_handle = cla_link->tx_queue_handle,
			.tx_queue_sem = cla_link->tx_queue_sem,
		};
	}

	hal_semaphore_release(mtcp_config->param_htab_sem);
	return (struct cla_tx_queue){ NULL, NULL };
}

static enum ud3tn_result mtcp_start_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	(void)eid;
	struct mtcp_config *const mtcp_config = (struct mtcp_config *)config;

	hal_semaphore_take_blocking(mtcp_config->param_htab_sem);
	struct mtcp_contact_parameters *const param = get_contact_parameters(
		config,
		cla_addr
	);

	if (param) {
		LOGF("MTCP: Associating open connection with \"%s\" to new contact",
		     cla_addr);
		param->in_contact = true;
		hal_semaphore_release(mtcp_config->param_htab_sem);
		return UD3TN_OK;
	}

	launch_connection_management_task(mtcp_config, -1, cla_addr);
	hal_semaphore_release(mtcp_config->param_htab_sem);

	return UD3TN_OK;
}

static enum ud3tn_result mtcp_end_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	(void)eid;
	struct mtcp_config *const mtcp_config = (struct mtcp_config *)config;

	hal_semaphore_take_blocking(mtcp_config->param_htab_sem);
	struct mtcp_contact_parameters *const param = get_contact_parameters(
		config,
		cla_addr
	);

	if (param && param->in_contact) {
		LOGF("MTCP: Marking open connection with \"%s\" as opportunistic",
		     cla_addr);
		param->in_contact = false;
		if (CLA_MTCP_CLOSE_AFTER_CONTACT && param->socket >= 0) {
			LOGF("MTCP: Terminating connection with \"%s\"",
			     cla_addr);
			close(param->socket);
		}
	}

	hal_semaphore_release(mtcp_config->param_htab_sem);

	return UD3TN_OK;
}

void mtcp_begin_packet(struct cla_link *link, size_t length)
{
	struct cla_tcp_link *const tcp_link = (struct cla_tcp_link *)link;

	// A previous operation may have canceled the sending process.
	if (!link->active)
		return;

	const size_t BUFFER_SIZE = 9; // max. for uint64_t
	uint8_t buffer[BUFFER_SIZE];

	const size_t hdr_len = mtcp_encode_header(buffer, BUFFER_SIZE, length);

	if (tcp_send_all(tcp_link->connection_socket, buffer, hdr_len) == -1) {
		LOG("mtcp: Error during sending. Data discarded.");
		link->config->vtable->cla_disconnect_handler(link);
	}
}

void mtcp_end_packet(struct cla_link *link)
{
	// STUB
	(void)link;
}

void mtcp_send_packet_data(
	struct cla_link *link, const void *data, const size_t length)
{
	struct cla_tcp_link *const tcp_link = (struct cla_tcp_link *)link;

	// A previous operation may have canceled the sending process.
	if (!link->active)
		return;

	if (tcp_send_all(tcp_link->connection_socket, data, length) == -1) {
		LOG("mtcp: Error during sending. Data discarded.");
		link->config->vtable->cla_disconnect_handler(link);
	}
}

const struct cla_vtable mtcp_vtable = {
	.cla_name_get = mtcp_name_get,
	.cla_launch = mtcp_launch,
	.cla_mbs_get = mtcp_mbs_get,

	.cla_get_tx_queue = mtcp_get_tx_queue,
	.cla_start_scheduled_contact = mtcp_start_scheduled_contact,
	.cla_end_scheduled_contact = mtcp_end_scheduled_contact,

	.cla_begin_packet = mtcp_begin_packet,
	.cla_end_packet = mtcp_end_packet,
	.cla_send_packet_data = mtcp_send_packet_data,

	.cla_rx_task_reset_parsers = mtcp_reset_parsers,
	.cla_rx_task_forward_to_specific_parser =
		mtcp_forward_to_specific_parser,

	.cla_read = cla_tcp_read,

	.cla_disconnect_handler = cla_tcp_disconnect_handler,
};

static enum ud3tn_result mtcp_init(
	struct mtcp_config *config,
	const char *node, const char *service,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	/* Initialize base_config */
	if (cla_tcp_config_init(&config->base,
				bundle_agent_interface) != UD3TN_OK)
		return UD3TN_FAIL;

	/* set base_config vtable */
	config->base.base.vtable = &mtcp_vtable;

	htab_init(&config->param_htab, CLA_TCP_PARAM_HTAB_SLOT_COUNT,
		  config->param_htab_elem);

	config->param_htab_sem = hal_semaphore_init_binary();
	hal_semaphore_release(config->param_htab_sem);

	/* Start listening */
	if (cla_tcp_listen(&config->base, node, service,
			   CLA_TCP_MULTI_BACKLOG) != UD3TN_OK)
		return UD3TN_FAIL;

	return UD3TN_OK;
}

struct cla_config *mtcp_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (option_count != 2) {
		LOG("mtcp: Options format has to be: <IP>,<PORT>");
		return NULL;
	}

	struct mtcp_config *config = malloc(sizeof(struct mtcp_config));

	if (!config) {
		LOG("mtcp: Memory allocation failed!");
		return NULL;
	}

	if (mtcp_init(config, options[0], options[1],
		      bundle_agent_interface) != UD3TN_OK) {
		free(config);
		LOG("mtcp: Initialization failed!");
		return NULL;
	}

	return &config->base.base;
}
