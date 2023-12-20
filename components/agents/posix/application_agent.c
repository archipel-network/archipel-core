// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

#include "aap/aap.h"
#include "aap/aap_parser.h"
#include "aap/aap_serializer.h"

#include "agents/application_agent.h"

#include "bundle7/parser.h"

#include "cla/posix/cla_tcp_util.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"
#include "platform/hal_time.h"
#include "platform/hal_types.h"

#include "platform/posix/pipe_queue_util.h"
#include "platform/posix/socket_util.h"

#include "ud3tn/agent_util.h"
#include "ud3tn/common.h"
#include "ud3tn/bundle.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/eid.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <errno.h>
#include <poll.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>

// The BIBE BPDU type code, which was different in BIBE draft v1 (used by ION).
#if defined(BIBE_CL_DRAFT_1_COMPATIBILITY) && BIBE_CL_DRAFT_1_COMPATIBILITY != 0
static const uint8_t BIBE_TYPECODE = 7;
#else // BIBE_CL_DRAFT_1_COMPATIBILITY
static const uint8_t BIBE_TYPECODE = 3;
#endif // BIBE_CL_DRAFT_1_COMPATIBILITY

struct application_agent_config {
	const struct bundle_agent_interface *bundle_agent_interface;

	uint8_t bp_version;
	uint64_t lifetime_ms;

	int listen_socket;
};

struct application_agent_comm_config {
	struct application_agent_config *parent;
	int socket_fd;
	int bundle_pipe_fd[2];
	char *registered_agent_id;
	uint64_t last_bundle_timestamp_ms;
	uint64_t last_bundle_sequence_number;
};

// forward declaration
static void application_agent_comm_task(void *const param);

static void application_agent_listener_task(void *const param)
{
	struct application_agent_config *const config = (
		(struct application_agent_config *)param
	);

	for (;;) {
		struct sockaddr_storage incoming;
		socklen_t incoming_addr_len = sizeof(struct sockaddr_storage);
		char addrstrbuf[sizeof(struct sockaddr_storage)];

		int conn_fd = accept(
			config->listen_socket,
			(struct sockaddr *)&incoming,
			&incoming_addr_len
		);

		if (conn_fd == -1) {
			LOG_ERRNO("AppAgent", "accept()", errno);
			continue;
		}

		switch (incoming.ss_family) {
		case AF_UNIX:
			LOG_INFO("AppAgent: Accepted connection from UNIX Domain Socket.");
			break;
		case AF_INET: {
			struct sockaddr_in *in =
				(struct sockaddr_in *)&incoming;
			LOGF_INFO(
				"AppAgent: Accepted connection from '%s'.",
				inet_ntop(in->sin_family, &in->sin_addr,
					  addrstrbuf, sizeof(addrstrbuf))
			);
			break;
		}
		case AF_INET6: {
			struct sockaddr_in6 *in =
				(struct sockaddr_in6 *)&incoming;
			LOGF_INFO(
				"AppAgent: Accepted connection from '%s'.",
				inet_ntop(in->sin6_family, &in->sin6_addr,
					  addrstrbuf, sizeof(addrstrbuf))
			);
			break;
		}
		default:
			close(conn_fd);
			LOG_WARN("AppAgent: Unknown address family for incoming connection. Connection closed!");
			continue;
		}

		struct application_agent_comm_config *child_config = malloc(
			sizeof(struct application_agent_comm_config)
		);
		if (!child_config) {
			LOG_ERROR("AppAgent: Error allocating memory for config!");
			close(conn_fd);
			continue;
		}
		child_config->parent = config;
		child_config->socket_fd = conn_fd;
		child_config->last_bundle_timestamp_ms = 0;
		child_config->last_bundle_sequence_number = 0;
		child_config->registered_agent_id = NULL;

		if (pipe(child_config->bundle_pipe_fd) == -1) {
			LOG_ERRNO("AppAgent", "pipe()", errno);
			close(conn_fd);
			free(child_config);
			continue;
		}

		const enum ud3tn_result task_creation_result = hal_task_create(
			application_agent_comm_task,
			"app_comm_t",
			APPLICATION_AGENT_TASK_PRIORITY,
			child_config,
			DEFAULT_TASK_STACK_SIZE
		);

		if (task_creation_result != UD3TN_OK) {
			LOG_ERROR("AppAgent: Error starting comm. task!");
			close(conn_fd);
			free(child_config);
		}
	}
}

static size_t parse_aap(struct aap_parser *parser,
			const uint8_t *const input, const size_t length)
{
	size_t delta = 0;
	size_t consumed = 0;

	while (parser->status == PARSER_STATUS_GOOD && consumed < length) {
		delta = parser->parse(
			parser,
			input + consumed,
			length - consumed
		);
		if (!delta)
			break;
		consumed += delta;
	}

	return consumed;
}

static int send_message(const int socket_fd,
			const struct aap_message *const msg)
{
	struct tcp_write_to_socket_param wsp = {
		.socket_fd = socket_fd,
		.errno_ = 0,
	};

	aap_serialize(msg, tcp_write_to_socket, &wsp, true);
	if (wsp.errno_)
		LOG_ERRNO("AppAgent", "send()", wsp.errno_);
	return -wsp.errno_;
}

static void agent_msg_recv(struct bundle_adu data, void *param,
			   const void *bp_context)
{
	(void)bp_context;

	struct application_agent_comm_config *const config = (
		(struct application_agent_comm_config *)param
	);

	LOGF_DEBUG(
		"AppAgent: Got Bundle for sink \"%s\" from \"%s\", forwarding.",
		config->registered_agent_id,
		data.source
	);

	if (pipeq_write_all(config->bundle_pipe_fd[1],
			    &data, sizeof(struct bundle_adu)) <= 0) {
		LOG_ERRNO("AppAgent", "write()", errno);
		bundle_adu_free_members(data);
	}
}

static int register_sink(char *sink_identifier,
	struct application_agent_comm_config *config)
{
	return bundle_processor_perform_agent_action(
		config->parent->bundle_agent_interface->bundle_signaling_queue,
		BP_SIGNAL_AGENT_REGISTER,
		(struct agent){
			.sink_identifier = sink_identifier,
			.callback = agent_msg_recv,
			.param = config,
		},
		true
	);
}

static void deregister_sink(struct application_agent_comm_config *config)
{
	if (!config->registered_agent_id)
		return;

	LOGF_INFO(
		"AppAgent: De-registering agent ID \"%s\".",
		config->registered_agent_id
	);

	bundle_processor_perform_agent_action(
		config->parent->bundle_agent_interface->bundle_signaling_queue,
		BP_SIGNAL_AGENT_DEREGISTER,
		(struct agent){
			.sink_identifier = config->registered_agent_id,
		},
		true
	);

	free(config->registered_agent_id);
	config->registered_agent_id = NULL;
}

static uint64_t allocate_sequence_number(
	struct application_agent_comm_config *const config,
	const uint64_t time_ms)
{
	if (config->last_bundle_timestamp_ms == time_ms)
		return ++config->last_bundle_sequence_number;

	config->last_bundle_timestamp_ms = time_ms;
	config->last_bundle_sequence_number = 1;

	return 1;
}

static int16_t process_aap_message(
	struct application_agent_comm_config *const config,
	struct aap_message msg)
{
	struct aap_message response = { .type = AAP_MESSAGE_INVALID };
	int16_t result = -1;

	// First, check validity as this is not done completely by the parser.
	if (!aap_message_is_valid(&msg))
		return result;

	switch (msg.type) {
	case AAP_MESSAGE_REGISTER:
		LOGF_INFO(
			"AppAgent: Received registration request for ID \"%s\".",
			msg.eid
		);

		deregister_sink(config);

		if (register_sink(msg.eid, config)) {
			LOG_INFO("AppAgent: Registration request declined.");
			response.type = AAP_MESSAGE_NACK;
		} else {
			config->registered_agent_id = msg.eid;
			msg.eid = NULL; // take over freeing of EID
			response.type = AAP_MESSAGE_ACK;
		}
		break;

	case AAP_MESSAGE_SENDBUNDLE:
	case AAP_MESSAGE_SENDBIBE:
		LOGF_DEBUG(
			"AppAgent: Received %s (l = %zu) for %s via AAP.",
			(
				msg.type == AAP_MESSAGE_SENDBIBE
				? "BIBE BPDU"
				: "bundle"
			),
			msg.payload_length,
			msg.eid
		);

		if (!config->registered_agent_id) {
			LOG_WARN("AppAgent: No agent ID registered, dropping!");
			break;
		}

		if (msg.type == AAP_MESSAGE_SENDBIBE) {
			LOG_DEBUG("AppAgent: ADU is a BPDU, prepending AR header!");

			const size_t ar_size = msg.payload_length + 2;
			uint8_t *const ar_bytes = malloc(ar_size);

			memcpy(
				ar_bytes + 2,
				msg.payload,
				msg.payload_length
			);
			ar_bytes[0] = 0x82;          // CBOR array of length 2
			ar_bytes[1] = BIBE_TYPECODE; // Integer (record type)

			free(msg.payload);
			msg.payload = ar_bytes;
			msg.payload_length = ar_size;
		}

		const uint64_t time_ms = hal_time_get_timestamp_ms();
		const uint64_t seqnum = allocate_sequence_number(
			config,
			time_ms
		);
		struct bundle *bundle = agent_create_forward_bundle(
			config->parent->bundle_agent_interface,
			config->parent->bp_version,
			config->registered_agent_id,
			msg.eid,
			time_ms,
			seqnum,
			config->parent->lifetime_ms,
			msg.payload,
			msg.payload_length,
			(
				msg.type == AAP_MESSAGE_SENDBIBE
				? BUNDLE_FLAG_ADMINISTRATIVE_RECORD
				: 0
			)
		);
		// Pointer responsibility was taken by create_forward_bundle
		msg.payload = NULL;

		if (!bundle) {
			LOG_ERROR("AppAgent: Bundle creation failed!");
			response.type = AAP_MESSAGE_NACK;
		} else {
			LOGF_DEBUG("AppAgent: Injected new bundle %p.", bundle);
			response.type = AAP_MESSAGE_SENDCONFIRM;
			response.bundle_id = (
				1ULL << 63 | // reserved bit MUST be 1
				0ULL << 62 | // Format: with-timestamp
				// 46-bit creation timestamp, see #60
				(time_ms & 0x00003FFFFFFFFFFFULL) << 16 |
				(seqnum & 0xFFFF) // 16-bit seqnum
			);
		}

		break;

	case AAP_MESSAGE_CANCELBUNDLE:
		LOGF_DEBUG(
			"AppAgent: Received bundle cancellation request for bundle #%llu.",
			(uint64_t)msg.bundle_id
		);

		// TODO: Signal to Bundle Processor + implement handling of it
		LOG_ERROR("AppAgent: Bundle cancellation failed (NOT IMPLEMENTED)!");

		response.type = AAP_MESSAGE_NACK;
		break;

	case AAP_MESSAGE_PING:
		LOGF_DEBUG(
			"AppAgent: Received PING from \"%s\"",
			config->registered_agent_id
			? config->registered_agent_id
			: "<not registered>"
		);
		response = (struct aap_message){ .type = AAP_MESSAGE_ACK };
		break;

	default:
		LOGF_WARN(
			"AppAgent: Cannot handle AAP messages of type %d!",
			msg.type
		);
		break;
	}

	if (response.type != AAP_MESSAGE_INVALID)
		result = (int16_t)send_message(config->socket_fd, &response);

	// Free message contents
	aap_message_clear(&msg);
	return result;
}

static ssize_t receive_from_socket(
	struct application_agent_comm_config *const config,
	uint8_t *const rx_buffer, size_t bytes_available,
	struct aap_parser *const parser)
{
	ssize_t recv_result;
	size_t bytes_parsed;

	// Receive from socket into buffer
	recv_result = recv(
		config->socket_fd,
		rx_buffer + bytes_available,
		APPLICATION_AGENT_RX_BUFFER_SIZE - bytes_available,
		0
	);
	if (recv_result <= 0) {
		if (recv_result < 0)
			LOG_ERRNO("AppAgent", "recv()", errno);
		return -1;
	}
	bytes_available += recv_result;

	// Try parsing current buffer contents and process message
	bytes_parsed = parse_aap(parser, rx_buffer, bytes_available);
	ASSERT(bytes_parsed <= bytes_available);
	if (parser->status != PARSER_STATUS_GOOD) {
		if (parser->status == PARSER_STATUS_DONE)
			process_aap_message(
				config,
				aap_parser_extract_message(parser)
			);
		else
			LOG_ERROR("AppAgent: Failed parsing received AAP message!");
		aap_parser_reset(parser);
	}

	// Shift remaining bytes in buffer to the left
	if (bytes_parsed != bytes_available)
		memmove(rx_buffer, rx_buffer + bytes_parsed,
			bytes_available - bytes_parsed);
	bytes_available -= bytes_parsed;

	return bytes_available;
}

static int send_bundle(const int socket_fd, struct bundle_adu data)
{
	const struct aap_message bundle_msg = {
		.type = (
			data.proc_flags == BUNDLE_FLAG_ADMINISTRATIVE_RECORD
			? AAP_MESSAGE_RECVBIBE
			: AAP_MESSAGE_RECVBUNDLE
		),
		.eid = data.source,
		.eid_length = strlen(data.source),
		.payload = data.payload,
		.payload_length = data.length,
	};
	const int send_result = send_message(socket_fd, &bundle_msg);

	bundle_adu_free_members(data);
	return send_result;
}

static void shutdown_bundle_pipe(int bundle_pipe_fd[2])
{
	struct bundle_adu data;

	while (poll_recv_timeout(bundle_pipe_fd[0], 0)) {
		if (pipeq_read_all(bundle_pipe_fd[0],
				   &data, sizeof(struct bundle_adu)) <= 0) {
			LOG_ERRNO("AppAgent", "read()", errno);
			break;
		}

		LOGF_WARN(
			"AppAgent: Dropping unsent bundle from '%s'.",
			data.source
		);
		bundle_adu_free_members(data);
	}

	close(bundle_pipe_fd[0]);
	close(bundle_pipe_fd[1]);
}

static void application_agent_comm_task(void *const param)
{
	struct application_agent_comm_config *const config = (
		(struct application_agent_comm_config *)param
	);

	char *local_eid = config->parent->bundle_agent_interface->local_eid;
	const struct aap_message welcome = {
		.type = AAP_MESSAGE_WELCOME,
		.eid = local_eid,
		.eid_length = strlen(local_eid),
	};

	if (send_message(config->socket_fd, &welcome))
		goto done;

	struct pollfd pollfd[2];

	pollfd[0].events = POLLIN;
	pollfd[0].fd = config->socket_fd;
	pollfd[1].events = POLLIN;
	pollfd[1].fd = config->bundle_pipe_fd[0];

	uint8_t rx_buffer[APPLICATION_AGENT_RX_BUFFER_SIZE];
	size_t bytes_available = 0;
	struct aap_parser parser;

	aap_parser_init(&parser);
	parser.max_payload_length = BUNDLE_MAX_SIZE;

	for (;;) {
		if (poll(pollfd, ARRAY_LENGTH(pollfd), -1) == -1) {
			LOG_ERRNO("AppAgent", "poll()", errno);
			// Try again if interrupted by a signal, else fail.
			if (errno == EINTR)
				continue;
			break;
		}
		if (pollfd[0].revents & POLLERR ||
		    pollfd[1].revents & POLLERR) {
			LOG_WARN("AppAgent: Socket error (e.g. TCP RST) detected.");
			break;
		}
		if (pollfd[0].revents & POLLIN) {
			const ssize_t new_bytes_available = receive_from_socket(
				config,
				rx_buffer,
				bytes_available,
				&parser
			);

			if (new_bytes_available == -1)
				break;
			bytes_available = new_bytes_available;
		}
		if (pollfd[1].revents & POLLIN) {
			struct bundle_adu data;

			if (pipeq_read_all(config->bundle_pipe_fd[0],
					   &data,
					   sizeof(struct bundle_adu)) <= 0) {
				LOG_ERRNO("AppAgent", "read()", errno);
				break;
			}
			if (send_bundle(config->socket_fd, data) < 0)
				break;
		}
	}

	aap_parser_deinit(&parser);
done:
	deregister_sink(config);
	shutdown_bundle_pipe(config->bundle_pipe_fd);
	shutdown(config->socket_fd, SHUT_RDWR);
	close(config->socket_fd);
	free(config);
	LOG_INFO("AppAgent: Closed connection.");
}

struct application_agent_config *application_agent_setup(
	const struct bundle_agent_interface *bundle_agent_interface,
	const char *socket_path,
	const char *node, const char *service,
	const uint8_t bp_version, uint64_t lifetime_ms)
{
	struct application_agent_config *const config = malloc(
		sizeof(struct application_agent_config)
	);

	if (!config) {
		LOG_ERROR("AppAgent: Error allocating memory for task config!");
		return NULL;
	}

	if (node && service)
		config->listen_socket =
			create_tcp_socket(node, service, false, NULL);
	else
		config->listen_socket =
			create_unix_domain_socket(socket_path);

	if (config->listen_socket < 0)  {
		LOG_ERROR("AppAgent: Error binding to provided address!");
		free(config);
		return NULL;
	}

	if (listen(config->listen_socket, APPLICATION_AGENT_BACKLOG) < 0) {
		LOG_ERRNO_ERROR(
			"AppAgent",
			"Error listening on provided address!",
			errno
		);
		free(config);
		return NULL;
	}

	if (node && service)
		LOGF_INFO("AppAgent: Listening on [%s]:%s", node, service);
	else
		LOGF_INFO("AppAgent: Listening on %s", socket_path);

	config->bundle_agent_interface = bundle_agent_interface;
	config->bp_version = bp_version;
	config->lifetime_ms = lifetime_ms;

	const enum ud3tn_result task_creation_result = hal_task_create(
		application_agent_listener_task,
		"app_listener_t",
		APPLICATION_AGENT_TASK_PRIORITY,
		config,
		DEFAULT_TASK_STACK_SIZE
	);

	if (task_creation_result != UD3TN_OK) {
		LOG_ERROR("AppAgent: Error creating listener task!");
		free(config);
		return NULL;
	}

	return config;
}
