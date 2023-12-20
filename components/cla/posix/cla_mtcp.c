// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"
#include "cla/mtcp_proto.h"
#include "cla/posix/cla_mtcp.h"
#include "cla/posix/cla_tcp_common.h"
#include "cla/posix/cla_tcp_util.h"

#include "bundle6/parser.h"
#include "bundle7/parser.h"

#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"
#include "platform/hal_types.h"

#include "ud3tn/bundle_processor.h"
#include "ud3tn/cmdline.h"
#include "ud3tn/common.h"
#include "ud3tn/result.h"
#include "ud3tn/simplehtab.h"

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

	Semaphore_t param_semphr; // rw access to params struct

	char *cla_sock_addr;

	bool is_outgoing;
	bool in_contact;
	int connect_attempt;

	int socket;
};


static enum ud3tn_result handle_established_connection(
	struct mtcp_contact_parameters *const param)
{
	struct mtcp_config *const mtcp_config = param->config;

	if (cla_tcp_link_init(&param->link.base, param->socket,
			      &mtcp_config->base, param->cla_sock_addr,
			      param->is_outgoing)
			!= UD3TN_OK) {
		LOG_ERROR("MTCP: Error initializing CLA link!");
		return UD3TN_FAIL;
	}

	// For the duration of the TX and RX tasks being executed, we hand
	// the access of the parameters structure to these tasks. This means
	// that we have to unlock the semaphore (it is locked beforehand
	// starting at its creation in `launch_connection_management_task`).
	// This way, the RX and TX task can lock and safely access the struct
	// if needed. `cla_link_wait` will return after both tasks have
	// terminated gracefully, thus, we can re-lock it afterwards.
	hal_semaphore_release(param->param_semphr);
	cla_link_wait(&param->link.base.base);
	hal_semaphore_take_blocking(param->config->param_htab_sem);
	hal_semaphore_take_blocking(param->param_semphr);
	cla_link_cleanup(&param->link.base.base);
	hal_semaphore_release(param->config->param_htab_sem);

	return UD3TN_OK;
}

static void mtcp_link_management_task(void *p)
{
	struct mtcp_contact_parameters *const param = p;

	ASSERT(param->cla_sock_addr != NULL);
	if (!param->cla_sock_addr) {
		LOG_ERROR("MTCP: Empty CLA address, cannot launch management task");
		goto fail;
	}

	// NOTE: This loop always has to run at least once (opport. contacts)
	do {
		if (param->socket > 0) {
			// NOTE the following function releases and re-locks sem
			handle_established_connection(param);
			param->connect_attempt = 0;
			param->socket = -1;
		} else {
			if (param->cla_sock_addr[0] == '\0') {
				LOG_ERROR("MTCP: Empty CLA address, cannot initiate connection");
				break;
			}
			ASSERT(param->socket < 0);
			hal_semaphore_release(param->param_semphr);

			const int socket = cla_tcp_connect_to_cla_addr(
				param->cla_sock_addr, // only used by us
				NULL
			);

			hal_semaphore_take_blocking(param->param_semphr);
			if (socket < 0) {
				if (++param->connect_attempt >
						CLA_TCP_MAX_RETRY_ATTEMPTS) {
					LOG_WARN("MTCP: Final retry failed.");
					break;
				}
				LOGF_INFO(
					"MTCP: Delayed retry %d of %d in %d ms",
					param->connect_attempt,
					CLA_TCP_MAX_RETRY_ATTEMPTS,
					CLA_TCP_RETRY_INTERVAL_MS
				);
				hal_semaphore_release(param->param_semphr);
				hal_task_delay(CLA_TCP_RETRY_INTERVAL_MS);
				hal_semaphore_take_blocking(
					param->param_semphr
				);
				continue;
			}
			param->socket = socket;
			LOGF_INFO(
				"MTCP: Connected successfully to \"%s\"",
				param->cla_sock_addr
			);
		}
	} while (param->in_contact);
	LOGF_INFO(
		"MTCP: Terminating contact link manager for \"%s\"",
		param->cla_sock_addr
	);

	// Unblock parallel threads that thought to re-use the link while
	// telling them it is not usable anymore by invalidating the socket.
	param->in_contact = false;
	param->socket = -1;
	hal_semaphore_release(param->param_semphr);

	hal_semaphore_take_blocking(param->config->param_htab_sem);
	htab_remove(&param->config->param_htab, param->cla_sock_addr);
	hal_semaphore_release(param->config->param_htab_sem);

	hal_semaphore_take_blocking(param->param_semphr);

	mtcp_parser_reset(&param->link.mtcp_parser);
	free(param->cla_sock_addr);

fail:
	hal_semaphore_delete(param->param_semphr);
	free(param);
}

static void launch_connection_management_task(
	struct mtcp_config *const mtcp_config,
	const int sock, const char *cla_addr)
{
	ASSERT(cla_addr);
	struct mtcp_contact_parameters *contact_params =
		malloc(sizeof(struct mtcp_contact_parameters));

	if (!contact_params) {
		LOG_ERROR("MTCP: Failed to allocate memory!");
		return;
	}

	contact_params->config = mtcp_config;
	contact_params->connect_attempt = 0;

	if (sock < 0) {
		contact_params->cla_sock_addr = cla_get_connect_addr(
			cla_addr,
			"mtcp"
		);

		if (!contact_params->cla_sock_addr) {
			LOG_WARN("MTCP: Invalid address");
			free(contact_params);
			return;
		}

		contact_params->socket = -1;
		contact_params->in_contact = true;
		contact_params->is_outgoing = true;
	} else {
		contact_params->cla_sock_addr = strdup(cla_addr);

		if (!contact_params->cla_sock_addr) {
			LOG_ERROR("MTCP: Failed to allocate memory!");
			free(contact_params);
			return;
		}

		contact_params->socket = sock;
		contact_params->in_contact = false;
		contact_params->is_outgoing = false;
	}

	contact_params->param_semphr = hal_semaphore_init_binary();
	ASSERT(contact_params->param_semphr != NULL);
	if (contact_params->param_semphr == NULL) {
		LOG_ERROR("MTCP: Failed to create semaphore!");
		goto fail;
	}
	// NOTE that the binary semaphore (mutex) is locked after creation.

	mtcp_parser_reset(&contact_params->link.mtcp_parser);

	struct htab_entrylist *htab_entry = NULL;

	htab_entry = htab_add(
		&mtcp_config->param_htab,
		contact_params->cla_sock_addr,
		contact_params
	);
	if (!htab_entry) {
		LOG_ERROR("MTCP: Error creating htab entry!");
		goto fail_sem;
	}

	const enum ud3tn_result task_creation_result = hal_task_create(
		mtcp_link_management_task,
		contact_params
	);

	if (task_creation_result != UD3TN_OK) {
		LOG_ERROR("MTCP: Error creating management task!");
		if (htab_entry) {
			ASSERT(contact_params->cla_sock_addr);
			ASSERT(htab_remove(
				&mtcp_config->param_htab,
				contact_params->cla_sock_addr
			) == contact_params);
		}
		goto fail_sem;
	}

	return;

fail_sem:
	hal_semaphore_delete(contact_params->param_semphr);
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

	LOG_ERROR("MTCP: Unxepected failure to accept connection - abort!");
	abort();
}

static enum ud3tn_result mtcp_launch(struct cla_config *const config)
{
	return hal_task_create(
		mtcp_listener_task,
		config
	);
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

	if (!cla_sock_addr) {
		LOG_WARN("MTCP: Invalid address");
		return NULL;
	}

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

	if (param != NULL) {
		hal_semaphore_take_blocking(param->param_semphr);

		// Check that link is connected
		if (param->socket < 0) {
			hal_semaphore_release(param->param_semphr);
			hal_semaphore_release(mtcp_config->param_htab_sem);
			return (struct cla_tx_queue){ NULL, NULL };
		}

		struct cla_link *const cla_link = &param->link.base.base;

		hal_semaphore_take_blocking(cla_link->tx_queue_sem);
		hal_semaphore_release(param->param_semphr);
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

	if (param != NULL) {
		hal_semaphore_take_blocking(param->param_semphr);
		if (param->socket > 0 || param->in_contact) {
			LOGF_INFO(
				"MTCP: Associating open connection with \"%s\" to new contact",
				cla_addr
			);
			param->in_contact = true;

			const struct bundle_agent_interface *bai =
				config->bundle_agent_interface;
			const bool link_active = param->socket > 0;

			hal_semaphore_release(param->param_semphr);
			// Check that link is connected
			if (link_active) {
				bundle_processor_inform(
					bai->bundle_signaling_queue,
					(struct bundle_processor_signal) {
						.type = BP_SIGNAL_NEW_LINK_ESTABLISHED,
						.peer_cla_addr = strdup(
							cla_addr
						),
					}
				);
			}
			hal_semaphore_release(mtcp_config->param_htab_sem);
			return UD3TN_OK;
		}
		hal_semaphore_release(param->param_semphr);
		// Task is cleaning up already, just insert a new entry then.
		// NOTE this calls htab_remove earlier than the connection
		// management task but the 2nd call invoked by the latter
		// is no issue - it will not find the entry and return.
		htab_remove(&mtcp_config->param_htab, cla_addr);
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

	if (param != NULL) {
		hal_semaphore_take_blocking(param->param_semphr);

		struct cla_link *const link = &param->link.base.base;

		param->in_contact = false;
		if (param->socket > 0) {
			hal_semaphore_release(param->param_semphr);
			if (CLA_MTCP_CLOSE_AFTER_CONTACT) {
				LOGF_INFO(
					"MTCP: Terminating connection with \"%s\"",
					cla_addr
				);
				link->config->vtable->cla_disconnect_handler(
					link
				);
			} else {
				LOGF_INFO(
					"MTCP: Marking open connection with \"%s\" as opportunistic",
					cla_addr
				);
			}
		} else {
			hal_semaphore_release(param->param_semphr);
		}
	}

	hal_semaphore_release(mtcp_config->param_htab_sem);

	return UD3TN_OK;
}

void mtcp_begin_packet(struct cla_link *link, size_t length, char *cla_addr)
{
	struct cla_tcp_link *const tcp_link = (struct cla_tcp_link *)link;

	const size_t BUFFER_SIZE = 9; // max. for uint64_t
	uint8_t buffer[BUFFER_SIZE];

	const size_t hdr_len = mtcp_encode_header(buffer, BUFFER_SIZE, length);

	if (tcp_send_all(tcp_link->connection_socket, buffer, hdr_len) == -1) {
		LOG_WARN("MTCP: Error during sending. Data discarded.");
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

	if (tcp_send_all(tcp_link->connection_socket, data, length) == -1) {
		LOG_WARN("MTCP: Error during sending. Data discarded.");
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

	config->param_htab_sem = hal_semaphore_init_binary();
	if (!config->param_htab_sem)
		return UD3TN_FAIL;
	hal_semaphore_release(config->param_htab_sem);

	htab_init(&config->param_htab, CLA_TCP_PARAM_HTAB_SLOT_COUNT,
		  config->param_htab_elem);

	/* Start listening */
	if (cla_tcp_listen(&config->base, node, service,
			   CLA_TCP_MULTI_BACKLOG) != UD3TN_OK) {
		hal_semaphore_take_blocking(config->param_htab_sem);
		htab_trunc(&config->param_htab);
		hal_semaphore_delete(config->param_htab_sem);
		return UD3TN_FAIL;
	}

	return UD3TN_OK;
}

struct cla_config *mtcp_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (option_count != 2) {
		LOG_ERROR("MTCP: Options format has to be: <IP>,<PORT>");
		return NULL;
	}

	struct mtcp_config *config = malloc(sizeof(struct mtcp_config));

	if (!config) {
		LOG_ERROR("MTCP: Memory allocation failed!");
		return NULL;
	}

	if (mtcp_init(config, options[0], options[1],
		      bundle_agent_interface) != UD3TN_OK) {
		free(config);
		LOG_ERROR("MTCP: Initialization failed!");
		return NULL;
	}

	return &config->base.base;
}
