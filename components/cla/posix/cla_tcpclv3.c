// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"
#include "cla/posix/cla_tcp_common.h"
#include "cla/posix/cla_tcp_util.h"
#include "cla/posix/cla_tcpclv3.h"
#include "cla/posix/cla_tcpclv3_proto.h"

#include "bundle6/parser.h"
#include "bundle6/sdnv.h"
#include "bundle7/parser.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"

#include "ud3tn/bundle_processor.h"
#include "ud3tn/cmdline.h"
#include "ud3tn/common.h"
#include "ud3tn/eid.h"
#include "ud3tn/result.h"
#include "ud3tn/simplehtab.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>


struct tcpclv3_config {
	struct cla_tcp_config base;

	struct htab_entrylist *param_htab_elem[CLA_TCP_PARAM_HTAB_SLOT_COUNT];
	struct htab param_htab;
	Semaphore_t param_htab_sem;
};

enum TCPCLV3_STATE {
	// No socket created. Initial state. Delete without contact.
	TCPCLV3_INACTIVE,
	// Socket was created, now trying to connect. Delete after contact end.
	TCPCLV3_CONNECTING,
	// TCP connection is open and active. Handshake is being performed.
	// Starting point for incoming opportunistic connections.
	TCPCLV3_CONNECTED,
	// TCPCL handshake was performed. CLA Link and RX/TX tasks exist.
	TCPCLV3_ESTABLISHED,
};

struct tcpclv3_contact_parameters {
	// IMPORTANT: Though the link is kind-of-a-base-class, it is only
	// initialized iff state == TCPCLV3_ESTABLISHED and de-initialized
	// on changing to a "lower" state. By that, a pair of RX/TX tasks is
	// always and only associated to a single TCPCL session, which spares
	// us from establishing a whole lot of locking mechanisms.
	struct cla_tcp_link link;

	struct tcpclv3_config *config;

	Semaphore_t param_semphr; // for modifying ops on the params struct

	char *eid;
	char *cla_addr;

	int connect_attempt;

	int socket;

	enum TCPCLV3_STATE state;
	// CONNECTED or ESTABLISHED, but NOT associated to a planned contact.
	// true on incoming connections without contact or still-open
	// connections after a contact has ended.
	bool opportunistic;

	struct tcpclv3_parser tcpclv3_parser;
};

/*
 * MGMT
 */

static enum ud3tn_result cla_tcpclv3_perform_handshake(
	struct tcpclv3_contact_parameters *const param)
{
	// Send contact header

	const char *const local_eid =
		param->config->base.base.bundle_agent_interface->local_eid;
	size_t header_len;
	char *const header = cla_tcpclv3_generate_contact_header(
		local_eid,
		&header_len
	);
	const int socket = param->socket;

	if (!header)
		return UD3TN_FAIL;

	hal_semaphore_release(param->param_semphr);

	if (tcp_send_all(socket, header, header_len) == -1) {
		free(header);
		LOG_ERRNO("TCPCLv3", "send(header)", errno);
		hal_semaphore_take_blocking(param->param_semphr);
		return UD3TN_FAIL;
	}

	free(header);

	// Receive and decode header

	char header_buf[8];

	// NOTE: Currently we do not use the negotiated parameters as we
	// disable all optional features and thus do not have to check against
	// them.
	if (tcp_recv_all(socket, header_buf, 8) <= 0 ||
			memcmp(header_buf, "dtn!", 4) != 0 ||
			header_buf[4] < 0x03) {
		LOG_WARN("TCPCLv3: Did not receive proper \"dtn!\" magic!");
		hal_semaphore_take_blocking(param->param_semphr);
		return UD3TN_FAIL;
	}

	uint8_t cur_byte;
	struct sdnv_state sdnv_state;
	uint32_t peer_eid_len = 0;

	sdnv_reset(&sdnv_state);
	while (sdnv_state.status == SDNV_IN_PROGRESS &&
			recv(socket, &cur_byte, 1, 0) == 1)
		sdnv_read_u32(&sdnv_state, &peer_eid_len, cur_byte);
	if (sdnv_state.status != SDNV_DONE) {
		LOG_WARN("TCPCLv3: Error receiving EID length SDNV!");
		hal_semaphore_take_blocking(param->param_semphr);
		return UD3TN_FAIL;
	}

	char *eid_buf = malloc(peer_eid_len + 1);

	if (!eid_buf) {
		LOGF_ERROR(
			"TCPCLv3: Error allocating memory (%u byte(s)) for EID!",
			peer_eid_len
		);
		hal_semaphore_take_blocking(param->param_semphr);
		return UD3TN_FAIL;
	}

	if (tcp_recv_all(socket, eid_buf,
			 peer_eid_len) != (int64_t)peer_eid_len) {
		free(eid_buf);
		LOGF_WARN(
			"TCPCLv3: Error receiving peer EID of len %u byte(s)",
			peer_eid_len
		);
		hal_semaphore_take_blocking(param->param_semphr);
		return UD3TN_FAIL;
	}

	eid_buf[peer_eid_len] = '\0';
	if (validate_eid(eid_buf) != UD3TN_OK) {
		LOGF_WARN(
			"TCPCLv3: Received invalid peer EID of len %u: \"%s\"",
			peer_eid_len,
			eid_buf
		);
		free(eid_buf);
		hal_semaphore_take_blocking(param->param_semphr);
		return UD3TN_FAIL;
	}

	hal_semaphore_take_blocking(param->param_semphr);

	LOGF_INFO(
		"TCPCLv3: Handshake performed with \"%s\", reports EID \"%s\"",
		param->cla_addr ? param->cla_addr : "<incoming>",
		eid_buf
	);
	if (param->eid) {
		// We did already configure a value for the EID (via a contact)
		if (strcmp(param->eid, eid_buf) != 0) {
			LOGF_WARN(
				"TCPCLv3: EID \"%s\" differs from configured EID \"%s\", using own configuration",
				eid_buf,
				param->eid
			);
		}
		free(eid_buf);
	} else {
		param->eid = eid_buf;
	}

	return UD3TN_OK;
}

static enum ud3tn_result handle_established_connection(
	struct tcpclv3_contact_parameters *const param)
{
	struct tcpclv3_config *const tcpclv3_config = param->config;

	hal_semaphore_take_blocking(tcpclv3_config->param_htab_sem);

	// Check if there is another connection which is
	// a) trying to connect / establish (non-opportunistic)
	// b) already established
	struct tcpclv3_contact_parameters *const other =
		htab_get(&tcpclv3_config->param_htab, param->eid);

	if (other && other != param) {
		hal_semaphore_take_blocking(other->param_semphr);

		// Another connection exists. We replace it as primary
		// connection used for TX, assuming the newest connection is
		// always the one to be used.
		LOGF_INFO(
			"TCPCLv3: Taking over management of connection with \"%s\"",
			param->eid
		);
		htab_remove(&tcpclv3_config->param_htab, param->eid);
		if (!other->opportunistic) {
			// Take over the "planned" status
			other->opportunistic = true;
			param->opportunistic = false;
			if (!param->cla_addr) {
				param->cla_addr = other->cla_addr;
				other->cla_addr = NULL;
			}
		}

		hal_semaphore_release(other->param_semphr);
	}

	// Will do nothing if element exists - this is expected
	htab_add(&tcpclv3_config->param_htab, param->eid, param);

	param->state = TCPCLV3_ESTABLISHED;

	if (cla_tcp_link_init(&param->link, param->socket,
			      &tcpclv3_config->base, param->cla_addr,
			      true)
			!= UD3TN_OK) {
		LOG_ERROR("TCPCLv3: Error initializing CLA link!");
		param->state = TCPCLV3_CONNECTING;
		hal_semaphore_release(tcpclv3_config->param_htab_sem);
		return UD3TN_FAIL;
	}

	hal_semaphore_release(tcpclv3_config->param_htab_sem);
	// For the duration of the TX and RX tasks being executed, we hand
	// the access of the parameters structure to these tasks. This means
	// that we have to unlock the semaphore (it is locked beforehand
	// starting at its creation in `launch_connection_management_task`).
	// This way, the RX and TX task can lock and safely access the struct
	// if needed. `cla_link_wait` will return after both tasks have
	// terminated gracefully, thus, we can re-lock it afterwards.
	hal_semaphore_release(param->param_semphr);
	cla_link_wait(&param->link.base);
	hal_semaphore_take_blocking(param->config->param_htab_sem);
	hal_semaphore_take_blocking(param->param_semphr);
	cla_link_cleanup(&param->link.base);
	hal_semaphore_release(param->config->param_htab_sem);

	param->state = TCPCLV3_CONNECTING;
	return UD3TN_OK;
}

static void tcpclv3_link_management_task(void *p)
{
	struct tcpclv3_contact_parameters *const param = p;

	for (;;) {
		if (param->state == TCPCLV3_CONNECTING) {
			if (param->opportunistic || !param->cla_addr ||
			    param->cla_addr[0] == '\0') {
				LOG_WARN("TCPCLv3: No CLA address present, not initiating connection attempt");
				break;
			}

			hal_semaphore_release(param->param_semphr);

			cla_tcp_rate_limit_connection_attempts(
				&param->config->base
			);
			const int socket = cla_tcp_connect_to_cla_addr(
				param->cla_addr,
				"4556"
			);

			hal_semaphore_take_blocking(param->param_semphr);

			if (socket < 0) {
				if (++param->connect_attempt >
						CLA_TCP_MAX_RETRY_ATTEMPTS) {
					LOG_WARN("TCPCLv3: Final retry failed.");
					break;
				}
				LOGF_INFO("TCPCLv3: Delayed retry %d of %d in %d ms",
				     param->connect_attempt,
				     CLA_TCP_MAX_RETRY_ATTEMPTS,
				     CLA_TCP_RETRY_INTERVAL_MS);
				hal_semaphore_release(param->param_semphr);
				hal_task_delay(CLA_TCP_RETRY_INTERVAL_MS);
				hal_semaphore_take_blocking(
					param->param_semphr
				);
				continue;
			}
			LOGF_INFO("TCPCLv3: Connected successfully to \"%s\"",
			     param->cla_addr);
			param->socket = socket;
			param->state = TCPCLV3_CONNECTED;
		} else if (param->state == TCPCLV3_CONNECTED) {
			ASSERT(param->socket > 0);
			if (cla_tcpclv3_perform_handshake(param) == UD3TN_OK)
				handle_established_connection(param);
			if (param->opportunistic) {
				LOG_INFO("TCPCLv3: Link marked as opportunistic, not initiating reconnection attempt");
				break;
			}
			if (!param->cla_addr || param->cla_addr[0] == '\0') {
				LOG_INFO("TCPCLv3: No CLA address present, not initiating reconnection attempt");
				break;
			}
			param->state = TCPCLV3_CONNECTING;
			param->connect_attempt = 0;
		} else {
			// TCPCLV3_INACTIVE, TCPCLV3_ESTABLISHED
			// should never happen as we are not created or wait
			abort();
		}
	}
	LOGF_INFO(
		"TCPCLv3: Terminating contact link manager%s%s%s",
		param->eid ? " for \"" : "",
		param->eid ? param->eid : "",
		param->eid ? "\"" : ""
	);

	// Unblock parallel threads that thought to re-use the link while
	// telling them it is not usable anymore by invalidating the socket
	// and setting the state to "inactive".
	param->state = TCPCLV3_INACTIVE;
	param->socket = -1;
	hal_semaphore_release(param->param_semphr);

	// Remove from htab if there is an existing entry
	// NOTE it should be safe to do a read-only access to `param->eid` here
	// even withpout locking the semaphore as the value it is set and
	// released by the same control flow that we are in. Locking the
	// `param->param_semphr` could lead to deadlocks with threads holding
	// `param->config->param_htab_sem` here.
	if (param->eid) {
		hal_semaphore_take_blocking(param->config->param_htab_sem);
		// Only delete in case it is our own entry...
		if (htab_get(&param->config->param_htab, param->eid) == param)
			htab_remove(&param->config->param_htab, param->eid);
		hal_semaphore_release(param->config->param_htab_sem);
	}

	hal_semaphore_take_blocking(param->param_semphr);

	tcpclv3_parser_reset(&param->tcpclv3_parser);
	free(param->eid);
	free(param->cla_addr);
	hal_semaphore_delete(param->param_semphr);
	free(param);
}

static void launch_connection_management_task(
	struct tcpclv3_config *const tcpclv3_config, const int sock,
	const char *eid, const char *cla_addr)
{
	struct tcpclv3_contact_parameters *contact_params =
		malloc(sizeof(struct tcpclv3_contact_parameters));

	if (!contact_params) {
		LOG_ERROR("TCPCLv3: Failed to allocate memory!");
		return;
	}

	contact_params->config = tcpclv3_config;
	contact_params->connect_attempt = 0;

	if (sock < 0) {
		ASSERT(eid && cla_addr);
		if (!eid || !cla_addr) {
			LOG_ERROR("TCPCLv3: Invalid parameters!");
			free(contact_params);
			return;
		}
		contact_params->eid = strdup(eid);

		if (!contact_params->eid) {
			LOG_ERROR("TCPCLv3: Failed to copy EID!");
			free(contact_params);
			return;
		}

		contact_params->cla_addr = cla_get_connect_addr(
			cla_addr,
			"tcpclv3"
		);

		if (!contact_params->cla_addr) {
			LOG_ERROR("TCPCLv3: Invalid address");
			free(contact_params->eid);
			free(contact_params);
			return;
		}

		contact_params->socket = -1;
		contact_params->state = TCPCLV3_CONNECTING;
		contact_params->opportunistic = false;
	} else {
		ASSERT(!eid && !cla_addr);
		contact_params->eid = NULL;
		contact_params->cla_addr = NULL;
		contact_params->socket = sock;
		contact_params->state = TCPCLV3_CONNECTED;
		contact_params->opportunistic = true;
	}

	contact_params->param_semphr = hal_semaphore_init_binary();
	ASSERT(contact_params->param_semphr != NULL);
	if (contact_params->param_semphr == NULL) {
		LOG_ERROR("TCPCLv3: Failed to create semaphore!");
		goto fail;
	}
	// NOTE that the binary semaphore (mutex) is locked after creation.

	if (!tcpclv3_parser_init(&contact_params->tcpclv3_parser)) {
		LOG_ERROR("TCPCLv3: Error initializing parser!");
		goto fail_sem;
	}

	struct htab_entrylist *htab_entry = NULL;

	if (contact_params->eid) {
		htab_entry = htab_add(
			&tcpclv3_config->param_htab,
			contact_params->eid,
			contact_params
		);
		if (!htab_entry) {
			LOG_ERROR("TCPCLv3: Error creating htab entry!");
			goto fail_sem;
		}
	}

	const enum ud3tn_result task_creation_result = hal_task_create(
		tcpclv3_link_management_task,
		"tcpclv3_mgmt_t",
		CONTACT_MANAGEMENT_TASK_PRIORITY,
		contact_params,
		CONTACT_MANAGEMENT_TASK_STACK_SIZE
	);

	if (task_creation_result != UD3TN_OK) {
		LOG_ERROR("TCPCLv3: Error creating management task!");
		if (htab_entry) {
			ASSERT(contact_params->eid);
			ASSERT(htab_remove(
				&tcpclv3_config->param_htab,
				contact_params->eid
			) == contact_params);
		}
		goto fail_sem;
	}

	return;

fail_sem:
	hal_semaphore_delete(contact_params->param_semphr);
fail:
	free(contact_params->eid);
	free(contact_params->cla_addr);
	free(contact_params);
}

static struct tcpclv3_contact_parameters *get_contact_parameters(
	struct cla_config *config, const char *eid)
{
	struct tcpclv3_config *const tcpclv3_config =
		(struct tcpclv3_config *)config;

	return htab_get(&tcpclv3_config->param_htab, eid);
}

static void tcpclv3_listener_task(void *p)
{
	struct tcpclv3_config *const tcpclv3_config = p;
	int sock;

	for (;;) {
		sock = cla_tcp_accept_from_socket(
			&tcpclv3_config->base,
			tcpclv3_config->base.socket,
			NULL
		);
		if (sock == -1)
			break;

		hal_semaphore_take_blocking(tcpclv3_config->param_htab_sem);
		launch_connection_management_task(
			tcpclv3_config,
			sock,
			NULL,
			NULL
		);
		hal_semaphore_release(tcpclv3_config->param_htab_sem);
	}

	LOG_ERROR("TCPCLv3: Unxepected failure to accept connection - abort!");
	abort();
}

/*
 * API
 */

static enum ud3tn_result tcpclv3_launch(struct cla_config *const config)
{
	return hal_task_create(
		tcpclv3_listener_task,
		"tcpclv3_listen_t",
		CONTACT_LISTEN_TASK_PRIORITY,
		config,
		CONTACT_LISTEN_TASK_STACK_SIZE
	);
}

static const char *tcpclv3_name_get(void)
{
	return "tcpclv3";
}

static size_t tcpclv3_mbs_get(struct cla_config *const config)
{
	(void)config;
	return SIZE_MAX;
}

static struct cla_tx_queue tcpclv3_get_tx_queue(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	(void)cla_addr;
	struct tcpclv3_config *const tcpclv3_config =
		(struct tcpclv3_config *)config;

	hal_semaphore_take_blocking(tcpclv3_config->param_htab_sem);
	struct tcpclv3_contact_parameters *const param = get_contact_parameters(
		config,
		eid
	);

	if (param != NULL) {
		hal_semaphore_take_blocking(param->param_semphr);

		if (param->state != TCPCLV3_ESTABLISHED) {
			hal_semaphore_release(param->param_semphr);
			hal_semaphore_release(tcpclv3_config->param_htab_sem);
			return (struct cla_tx_queue){ NULL, NULL };
		}

		hal_semaphore_take_blocking(param->link.base.tx_queue_sem);
		hal_semaphore_release(param->param_semphr);
		hal_semaphore_release(tcpclv3_config->param_htab_sem);

		// Freed while trying to obtain it
		if (!param->link.base.tx_queue_handle)
			return (struct cla_tx_queue){ NULL, NULL };

		return (struct cla_tx_queue){
			.tx_queue_handle = param->link.base.tx_queue_handle,
			.tx_queue_sem = param->link.base.tx_queue_sem,
		};
	}

	hal_semaphore_release(tcpclv3_config->param_htab_sem);
	return (struct cla_tx_queue){ NULL, NULL };
}

static enum ud3tn_result tcpclv3_start_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	struct tcpclv3_config *const tcpclv3_config =
		(struct tcpclv3_config *)config;

	hal_semaphore_take_blocking(tcpclv3_config->param_htab_sem);
	struct tcpclv3_contact_parameters *const param = get_contact_parameters(
		config,
		eid
	);

	if (param != NULL) {
		hal_semaphore_take_blocking(param->param_semphr);
		if (param->state != TCPCLV3_INACTIVE) {
			LOGF_INFO(
				"TCPCLv3: Associating open connection with \"%s\" to new contact",
				eid
			);
			param->opportunistic = false;
			// Update CLA address in parameters
			if (param->cla_addr)
				free(param->cla_addr);
			param->cla_addr = cla_get_connect_addr(cla_addr, "tcpclv3");

			if (!param->cla_addr) {
				LOG_WARN("TCPCLv3: Invalid address");
				hal_semaphore_release(param->param_semphr);
				hal_semaphore_release(tcpclv3_config->param_htab_sem);
				return UD3TN_FAIL;
			}

			const struct bundle_agent_interface *bai =
				config->bundle_agent_interface;
			const bool link_active = (
				param->state == TCPCLV3_ESTABLISHED
			);

			hal_semaphore_release(param->param_semphr);
			// Even if it is no _new_ connection, we notify the BP task
			// (as long as the connection is already UP)
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

			hal_semaphore_release(tcpclv3_config->param_htab_sem);
			return UD3TN_OK;
		}
		hal_semaphore_release(param->param_semphr);
		// Task is cleaning up already, just insert a new entry then.
		// NOTE this calls htab_remove earlier than the connection
		// management task but the 2nd call invoked by the latter
		// is no issue - it will not find the entry and return.
		htab_remove(&tcpclv3_config->param_htab, cla_addr);
	}

	launch_connection_management_task(tcpclv3_config, -1, eid, cla_addr);
	hal_semaphore_release(tcpclv3_config->param_htab_sem);

	return UD3TN_OK;
}

static enum ud3tn_result tcpclv3_end_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	(void)cla_addr;
	struct tcpclv3_config *const tcpclv3_config =
		(struct tcpclv3_config *)config;

	hal_semaphore_take_blocking(tcpclv3_config->param_htab_sem);
	struct tcpclv3_contact_parameters *const param = get_contact_parameters(
		config,
		eid
	);

	if (param != NULL) {
		hal_semaphore_take_blocking(param->param_semphr);
		if (!param->opportunistic) {
			LOGF_INFO(
				"TCPCLv3: Marking active contact with \"%s\" as opportunistic",
				eid
			);
			param->opportunistic = true;
		}
		hal_semaphore_release(param->param_semphr);
	}

	hal_semaphore_release(tcpclv3_config->param_htab_sem);

	return UD3TN_OK;
}

/*
 * RX
 */

static void tcpclv3_reset_parsers(struct cla_link *link)
{
	struct tcpclv3_contact_parameters *const param =
		(struct tcpclv3_contact_parameters *)link;

	tcpclv3_parser_reset(&param->tcpclv3_parser);

	if (param->state == TCPCLV3_ESTABLISHED) {
		rx_task_reset_parsers(&link->rx_task_data);
		link->rx_task_data.cur_parser = &param->tcpclv3_parser.basedata;
	}
}

static size_t tcpclv3_forward_to_specific_parser(struct cla_link *link,
						 const uint8_t *buffer,
						 size_t length)
{
	struct tcpclv3_contact_parameters *const param =
		(struct tcpclv3_contact_parameters *)link;
	struct rx_task_data *const rx_data = &link->rx_task_data;

	ASSERT(param->state == TCPCLV3_ESTABLISHED);
	if (param->tcpclv3_parser.stage != TCPCLV3_FORWARD_BUNDLE)
		return tcpclv3_parser_read(
			&param->tcpclv3_parser,
			buffer,
			length
		);

	// We do not allow to parse more than the stated length.
	if (length > param->tcpclv3_parser.fragment_size)
		length = param->tcpclv3_parser.fragment_size;

	size_t result = 0;

	switch (rx_data->payload_type) {
	case PAYLOAD_UNKNOWN:
		result = select_bundle_parser_version(rx_data, buffer, length);
		if (result == 0)
			tcpclv3_reset_parsers(link);
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
		tcpclv3_reset_parsers(link);
		return 0;
	}

	ASSERT(result <= param->tcpclv3_parser.fragment_size);
	param->tcpclv3_parser.fragment_size -= result;

	// All done
	if (!param->tcpclv3_parser.fragment_size)
		tcpclv3_reset_parsers(link);

	return result;
}

/*
 * TX
 */

static void tcpclv3_begin_packet(struct cla_link *link, size_t length, char *cla_addr)
{
	struct tcpclv3_contact_parameters *const param =
		(struct tcpclv3_contact_parameters *)link;

	ASSERT(param->state == TCPCLV3_ESTABLISHED);

	uint8_t header_buffer[1 + MAX_SDNV_SIZE];

	// Set packet type to DATA_SEGMENT and set both start and end flags.
	header_buffer[0] = (
		TCPCLV3_TYPE_DATA_SEGMENT |
		TCPCLV3_FLAG_S |
		TCPCLV3_FLAG_E
	);

	// Calculate and set SDNV size of packet length.
	int sdnv_len = sdnv_write_u32(&header_buffer[1], length);

	if (tcp_send_all(param->link.connection_socket,
			 header_buffer, sdnv_len + 1) == -1) {
		LOG_ERRNO("TCPCLv3", "send(segment_header)",
			 errno);
		link->config->vtable->cla_disconnect_handler(link);
	}
}

static void tcpclv3_end_packet(struct cla_link *link)
{
	struct tcpclv3_contact_parameters *const param =
		(struct tcpclv3_contact_parameters *)link;

	ASSERT(param->state == TCPCLV3_ESTABLISHED);
	// STUB
	(void)param;
}

static void tcpclv3_send_packet_data(
	struct cla_link *link, const void *data, const size_t length)
{
	struct tcpclv3_contact_parameters *const param =
		(struct tcpclv3_contact_parameters *)link;

	ASSERT(param->state == TCPCLV3_ESTABLISHED);

	if (tcp_send_all(param->link.connection_socket, data, length) == -1) {
		LOG_ERRNO("TCPCLv3", "send()", errno);
		link->config->vtable->cla_disconnect_handler(link);
	}
}

/*
 * INIT
 */

const struct cla_vtable tcpclv3_vtable = {
	.cla_name_get = tcpclv3_name_get,
	.cla_launch = tcpclv3_launch,
	.cla_mbs_get = tcpclv3_mbs_get,

	.cla_get_tx_queue = tcpclv3_get_tx_queue,
	.cla_start_scheduled_contact = tcpclv3_start_scheduled_contact,
	.cla_end_scheduled_contact = tcpclv3_end_scheduled_contact,

	.cla_begin_packet = tcpclv3_begin_packet,
	.cla_end_packet = tcpclv3_end_packet,
	.cla_send_packet_data = tcpclv3_send_packet_data,

	.cla_rx_task_reset_parsers = tcpclv3_reset_parsers,
	.cla_rx_task_forward_to_specific_parser =
			&tcpclv3_forward_to_specific_parser,

	.cla_read = cla_tcp_read,

	.cla_disconnect_handler = cla_tcp_disconnect_handler,
};

static enum ud3tn_result tcpclv3_init(
	struct tcpclv3_config *config,
	const char *node, const char *service,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	/* Initialize base_config */
	if (cla_tcp_config_init(&config->base,
				bundle_agent_interface) != UD3TN_OK)
		return UD3TN_FAIL;

	/* set base_config vtable */
	config->base.base.vtable = &tcpclv3_vtable;

	config->param_htab_sem = hal_semaphore_init_binary();
	if (!config->param_htab_sem)
		return UD3TN_FAIL;
	hal_semaphore_release(config->param_htab_sem);

	htab_init(&config->param_htab, CLA_TCP_PARAM_HTAB_SLOT_COUNT,
		  config->param_htab_elem);

	/* Start listening */
	if (cla_tcp_listen(&config->base, node, service,
			   CLA_TCP_MULTI_BACKLOG)
			!= UD3TN_OK) {
		hal_semaphore_take_blocking(config->param_htab_sem);
		htab_trunc(&config->param_htab);
		hal_semaphore_delete(config->param_htab_sem);
		return UD3TN_FAIL;
	}

	return UD3TN_OK;
}

struct cla_config *tcpclv3_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (option_count != 2) {
		LOG_ERROR("TCPCLv3: Options format has to be: <IP>,<PORT>");
		return NULL;
	}

	struct tcpclv3_config *config = malloc(sizeof(struct tcpclv3_config));

	if (!config) {
		LOG_ERROR("TCPCLv3: Memory allocation failed!");
		return NULL;
	}

	if (tcpclv3_init(config, options[0], options[1],
			 bundle_agent_interface) != UD3TN_OK) {
		free(config);
		LOG_ERROR("TCPCLv3: Initialization failed!");
		return NULL;
	}

	return &config->base.base;
}
