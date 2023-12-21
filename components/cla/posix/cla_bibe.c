// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "aap/aap.h"
#include "aap/aap_parser.h"
#include "aap/aap_serializer.h"

#include "cla/bibe_proto.h"
#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"
#include "cla/posix/cla_bibe.h"
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
#include "ud3tn/eid.h"
#include "ud3tn/result.h"
#include "ud3tn/simplehtab.h"

#include <sys/socket.h>
#include <unistd.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char *CLA_NAME = "bibe";

struct bibe_config {
	struct cla_tcp_config base;

	struct htab_entrylist *param_htab_elem[CLA_TCP_PARAM_HTAB_SLOT_COUNT];
	struct htab param_htab;
	Semaphore_t param_htab_sem;

	const char *node;
	const char *service;
};

struct bibe_contact_parameters {
	// IMPORTANT: The cla_tcp_link is only initialized iff connected == true
	struct bibe_link link;

	struct bibe_config *config;

	Semaphore_t param_semphr; // rw access to params struct

	char *cla_sock_addr;
	const char *partner_eid;

	bool in_contact;

	int socket;

	struct cla_tcp_rate_limit_config rl_config;
};

static enum ud3tn_result handle_established_connection(
	struct bibe_contact_parameters *const param)
{
	struct bibe_config *const bibe_config = param->config;

	if (cla_tcp_link_init(&param->link.base, param->socket,
			      &bibe_config->base, param->cla_sock_addr,
			      true)
			!= UD3TN_OK) {
		LOG_ERROR("BIBE: Error initializing CLA link!");
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

static void bibe_link_management_task(void *p)
{
	struct bibe_contact_parameters *const param = p;

	ASSERT(param->cla_sock_addr != NULL);
	if (!param->cla_sock_addr) {
		LOG_WARN("BIBE: Empty CLA address, cannot launch management task");
		goto fail;
	}

	// NOTE: This loop always has to run at least once (opport. contacts)
	do {
		if (param->socket > 0) {
			// NOTE the following function releases and re-locks sem
			handle_established_connection(param);
			param->socket = -1;
		} else {
			if (param->cla_sock_addr[0] == '\0') {
				LOG_WARN("BIBE: Empty CLA address, cannot initiate connection");
				break;
			}
			ASSERT(param->socket < 0);
			hal_semaphore_release(param->param_semphr);

			if (cla_tcp_rate_limit_connection_attempts(
					&param->rl_config))
				break;
			const int socket = cla_tcp_connect_to_cla_addr(
				param->cla_sock_addr, // only used by us
				NULL
			);

			hal_semaphore_take_blocking(param->param_semphr);
			if (socket < 0)
				continue;
			param->socket = socket;

			const struct aap_message register_bibe = {
				.type = AAP_MESSAGE_REGISTER,
				.eid = (
					get_eid_scheme(param->partner_eid) == EID_SCHEME_IPN
					? "2925"
					: "bibe"
				),
				.eid_length = 4,
			};
			struct tcp_write_to_socket_param wsp = {
				.socket_fd = param->socket,
				.errno_ = 0,
			};

			aap_serialize(
				&register_bibe,
				tcp_write_to_socket,
				&wsp,
				true
			);
			if (wsp.errno_) {
				LOG_ERRNO("BIBE", "send()", wsp.errno_);
				close(param->socket);
				param->socket = -1;
				continue;
			}
			LOGF_INFO(
				"BIBE: Connected successfully to \"%s\"",
				param->cla_sock_addr
			);
		}
	} while (param->in_contact);

	LOGF_INFO(
		"BIBE: Terminating contact link manager for \"%s\"",
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

	aap_parser_reset(&param->link.aap_parser);
	free(param->cla_sock_addr);

fail:
	hal_semaphore_delete(param->param_semphr);
	free(param);
}

static void launch_connection_management_task(
	struct bibe_config *const bibe_config,
	const char *cla_addr, const char *eid)
{
	ASSERT(cla_addr);

	struct bibe_contact_parameters *contact_params =
		malloc(sizeof(struct bibe_contact_parameters));

	if (!contact_params) {
		LOG_ERROR("BIBE: Failed to allocate memory!");
		return;
	}

	contact_params->config = bibe_config;

	char *const cla_sock_addr = cla_get_connect_addr(cla_addr, CLA_NAME);

	if (!cla_sock_addr) {
		LOG_WARN("BIBE: Invalid address");
		free(contact_params);
		return;
	}

	char *const eid_delimiter = strchr(cla_sock_addr, '#');

	// If <connect-addr>#<lower-eid> is used (we find a '#' delimiter)
	if (eid_delimiter)
		eid_delimiter[0] = '\0'; // null-terminate after sock address

	contact_params->cla_sock_addr = cla_sock_addr;
	contact_params->partner_eid = eid;
	contact_params->socket = -1;
	contact_params->in_contact = true;
	contact_params->rl_config.last_connection_attempt_ms = 0;
	contact_params->rl_config.last_connection_attempt_no = 1;

	if (!contact_params->cla_sock_addr) {
		LOG_ERROR("BIBE: Failed to obtain CLA address!");
		goto fail;
	}

	contact_params->param_semphr = hal_semaphore_init_binary();
	ASSERT(contact_params->param_semphr != NULL);
	if (contact_params->param_semphr == NULL) {
		LOG_ERROR("BIBE: Failed to create semaphore!");
		goto fail;
	}
	// NOTE that the binary semaphore (mutex) is locked after creation.

	aap_parser_init(&contact_params->link.aap_parser);

	struct htab_entrylist *htab_entry = NULL;

	htab_entry = htab_add(
		&bibe_config->param_htab,
		contact_params->cla_sock_addr,
		contact_params
	);
	if (!htab_entry) {
		LOG_ERROR("BIBE: Error creating htab entry!");
		goto fail_sem;
	}

	const enum ud3tn_result task_creation_result = hal_task_create(
		bibe_link_management_task,
		contact_params
	);

	if (task_creation_result != UD3TN_OK) {
		LOG_ERROR("BIBE: Error creating management task!");
		if (htab_entry) {
			ASSERT(contact_params->cla_sock_addr);
			ASSERT(htab_remove(
				&bibe_config->param_htab,
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

static enum ud3tn_result bibe_launch(struct cla_config *const config)
{
	/* Since the BIBE CLA does not need a listener task, the bibe_launch
	 * function has pretty much no functionality.
	 * It could however be used to establish a "standard connection"
	 * if there's a predefined partner node.
	 */
	return UD3TN_OK;
}

static const char *bibe_name_get(void)
{
	return CLA_NAME;
}

size_t bibe_mbs_get(struct cla_config *const config)
{
	(void)config;
	return SIZE_MAX;
}

void bibe_reset_parsers(struct cla_link *const link)
{
	struct bibe_link *const bibe_link = (struct bibe_link *)link;

	rx_task_reset_parsers(&link->rx_task_data);

	aap_parser_reset(&bibe_link->aap_parser);
	link->rx_task_data.cur_parser = bibe_link->aap_parser.basedata;
}


size_t bibe_forward_to_specific_parser(struct cla_link *const link,
				       const uint8_t *const buffer,
				       const size_t length)
{
	struct rx_task_data *const rx_data = &link->rx_task_data;
	struct bibe_link *const bibe_link = (struct bibe_link *)link;
	size_t result = 0;

	rx_data->cur_parser = bibe_link->aap_parser.basedata;
	result = aap_parser_read(
		&bibe_link->aap_parser,
		buffer,
		length
	);


	if (bibe_link->aap_parser.status == PARSER_STATUS_DONE) {
		struct aap_message msg = aap_parser_extract_message(
			&bibe_link->aap_parser
		);

		// The only relevant message type is RECVBIBE, as the CLA
		// does not need to do anything with WELCOME or ACK messages.
		if (msg.type == AAP_MESSAGE_RECVBIBE) {
			// Parsing the BPDU
			struct bibe_protocol_data_unit bpdu;

			// Ensure it is always initialized, so we can safely
			// invoke free() below.
			bpdu.encapsulated_bundle = NULL;

			size_t err = bibe_parser_parse(
				msg.payload,
				msg.payload_length,
				&bpdu
			);

			if (err == 0 && bpdu.payload_length != 0) {
				// Parsing and forwarding the encapsulated
				// bundle
				bundle7_parser_read(
					&rx_data->bundle7_parser,
					bpdu.encapsulated_bundle,
					bpdu.payload_length
				);
			}

			free(bpdu.encapsulated_bundle);
		}

		aap_message_clear(&msg);
		bibe_reset_parsers(link);
	}
	return result;
}

/*
 * TX
 */

static struct bibe_contact_parameters *get_contact_parameters(
	struct cla_config *const config, const char *const cla_addr)
{
	struct bibe_config *const bibe_config =
		(struct bibe_config *)config;
	char *const cla_sock_addr = cla_get_connect_addr(cla_addr, CLA_NAME);

	if (!cla_sock_addr) {
		LOG_WARN("BIBE: Invalid address");
		return NULL;
	}

	char *const eid_delimiter = strchr(cla_sock_addr, '#');

	// If <connect-addr>#<lower-eid> is used (we find a '#' delimiter)
	if (eid_delimiter)
		eid_delimiter[0] = '\0'; // null-terminate after sock address

	struct bibe_contact_parameters *param = htab_get(
		&bibe_config->param_htab,
		cla_sock_addr
	);
	free(cla_sock_addr);
	return param;
}

static struct cla_tx_queue bibe_get_tx_queue(
	struct cla_config *const config, const char *const eid,
	const char *const cla_addr)
{
	(void)eid;

	struct bibe_config *const bibe_config = (struct bibe_config *)config;

	hal_semaphore_take_blocking(bibe_config->param_htab_sem);
	struct bibe_contact_parameters *const param = get_contact_parameters(
		config,
		cla_addr
	);
	const char *const dest_eid_delimiter = strchr(cla_addr, '#');
	const bool dest_eid_is_valid = (
		dest_eid_delimiter &&
		dest_eid_delimiter[0] != '\0' &&
		// The EID starts after the delimiter.
		validate_eid(&dest_eid_delimiter[1]) == UD3TN_OK
	);

	if (param && dest_eid_is_valid) {
		hal_semaphore_take_blocking(param->param_semphr);

		// Check that link is connected
		if (param->socket < 0) {
			hal_semaphore_release(param->param_semphr);
			hal_semaphore_release(bibe_config->param_htab_sem);
			return (struct cla_tx_queue){ NULL, NULL };
		}

		struct cla_link *const cla_link = &param->link.base.base;

		hal_semaphore_take_blocking(cla_link->tx_queue_sem);
		hal_semaphore_release(param->param_semphr);
		hal_semaphore_release(bibe_config->param_htab_sem);

		// Freed while trying to obtain it
		if (!cla_link->tx_queue_handle)
			return (struct cla_tx_queue){ NULL, NULL };

		return (struct cla_tx_queue){
			.tx_queue_handle = cla_link->tx_queue_handle,
			.tx_queue_sem = cla_link->tx_queue_sem,
		};
	}

	hal_semaphore_release(bibe_config->param_htab_sem);
	return (struct cla_tx_queue){ NULL, NULL };
}

static enum ud3tn_result bibe_start_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	(void)eid;

	struct bibe_config *const bibe_config = (struct bibe_config *)config;

	hal_semaphore_take_blocking(bibe_config->param_htab_sem);
	struct bibe_contact_parameters *const param = get_contact_parameters(
		config,
		cla_addr
	);

	if (param) {
		hal_semaphore_take_blocking(param->param_semphr);
		if (param->socket > 0 || param->in_contact) {
			LOGF_INFO(
				"BIBE: Associating open connection with \"%s\" to new contact",
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

			hal_semaphore_release(bibe_config->param_htab_sem);
			return UD3TN_OK;
		}
		hal_semaphore_release(param->param_semphr);
		// Task is cleaning up already, just insert a new entry then.
		// NOTE this calls htab_remove earlier than the connection
		// management task but the 2nd call invoked by the latter
		// is no issue - it will not find the entry and return.
		htab_remove(&bibe_config->param_htab, cla_addr);
	}

	launch_connection_management_task(bibe_config, cla_addr, eid);
	hal_semaphore_release(bibe_config->param_htab_sem);

	return UD3TN_OK;
}

static enum ud3tn_result bibe_end_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	(void)eid;

	struct bibe_config *const bibe_config = (struct bibe_config *)config;

	hal_semaphore_take_blocking(bibe_config->param_htab_sem);
	struct bibe_contact_parameters *const param = get_contact_parameters(
		config,
		cla_addr
	);

	if (param != NULL) {
		hal_semaphore_take_blocking(param->param_semphr);
		param->in_contact = false;
		if (param->socket >= 0) {
			LOGF_INFO(
				"BIBE: Terminating connection with \"%s\"",
				cla_addr
			);
			// Shutting down the socket to force the lower layers
			// Application Agent to deregister the "bibe" sink.
			shutdown(param->socket, SHUT_RDWR);
			close(param->socket);
		}
		hal_semaphore_release(param->param_semphr);
	}

	hal_semaphore_release(bibe_config->param_htab_sem);

	return UD3TN_OK;
}

void bibe_begin_packet(struct cla_link *link, size_t length, char *cla_addr)
{
	struct cla_tcp_link *const tcp_link = (struct cla_tcp_link *)link;

	// Init strtok and get cla address
	const char *dest_eid = strchr(cla_addr, '#');

	ASSERT(dest_eid);
	ASSERT(dest_eid[0] != '\0' && dest_eid[1] != '\0');
	dest_eid = &dest_eid[1]; // EID starts _after_ the '#'

	struct bibe_header hdr;

	hdr = bibe_encode_header(dest_eid, length);

	if (tcp_send_all(tcp_link->connection_socket,
			 hdr.data, hdr.hdr_len) == -1) {
		LOG_ERROR("BIBE: Error during sending. Data discarded.");
		link->config->vtable->cla_disconnect_handler(link);
	}

	free(hdr.data);
}

void bibe_end_packet(struct cla_link *link)
{
	// STUB
	(void)link;
}

void bibe_send_packet_data(
	struct cla_link *link, const void *data, const size_t length)
{
	struct cla_tcp_link *const tcp_link = (struct cla_tcp_link *)link;

	if (tcp_send_all(tcp_link->connection_socket, data, length) == -1) {
		LOG_ERROR("BIBE: Error during sending. Data discarded.");
		link->config->vtable->cla_disconnect_handler(link);
	}
}

const struct cla_vtable bibe_vtable = {
	.cla_name_get = bibe_name_get,
	.cla_launch = bibe_launch,
	.cla_mbs_get = bibe_mbs_get,

	.cla_get_tx_queue = bibe_get_tx_queue,
	.cla_start_scheduled_contact = bibe_start_scheduled_contact,
	.cla_end_scheduled_contact = bibe_end_scheduled_contact,

	.cla_begin_packet = bibe_begin_packet,
	.cla_end_packet = bibe_end_packet,
	.cla_send_packet_data = bibe_send_packet_data,

	.cla_rx_task_reset_parsers = bibe_reset_parsers,
	.cla_rx_task_forward_to_specific_parser =
		bibe_forward_to_specific_parser,

	.cla_read = cla_tcp_read,

	.cla_disconnect_handler = cla_tcp_disconnect_handler,
};

static enum ud3tn_result bibe_init(
	struct bibe_config *config,
	const char *node, const char *service,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	// Initialize base_config
	if (cla_tcp_config_init(&config->base,
				bundle_agent_interface) != UD3TN_OK)
		return UD3TN_FAIL;

	// Set base_config vtable
	config->base.base.vtable = &bibe_vtable;

	config->param_htab_sem = hal_semaphore_init_binary();
	if (!config->param_htab_sem)
		return UD3TN_FAIL;
	hal_semaphore_release(config->param_htab_sem);

	htab_init(&config->param_htab, CLA_TCP_PARAM_HTAB_SLOT_COUNT,
		  config->param_htab_elem);

	config->node = node;
	config->service = service;

	return UD3TN_OK;
}

struct cla_config *bibe_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	struct bibe_config *config = malloc(sizeof(struct bibe_config));

	if (!config) {
		LOG_ERROR("BIBE: Memory allocation failed!");
		return NULL;
	}
	// TODO: Allow for passing of options indicating
	// which lower layer to connect to without scheduling
	// a contact? E.g. localhost:4242
	if (bibe_init(config, options[0], options[1],
		      bundle_agent_interface) != UD3TN_OK) {
		free(config);
		LOG_ERROR("BIBE: Initialization failed!");
		return NULL;
	}

	return &config->base.base;
}
