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

#include "ud3tn/agent_util.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
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

#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>

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
			LOGERROR("AppAgent", "accept()", errno);
			continue;
		}

		switch (incoming.ss_family) {
		case AF_UNIX:
			LOG("AppAgent: Accepted connection from UNIX Domain Socket.");
			break;
		case AF_INET: {
			struct sockaddr_in *in =
				(struct sockaddr_in *)&incoming;
			LOGF("AppAgent: Accepted connection from '%s'.",
			     inet_ntop(in->sin_family, &in->sin_addr,
				       addrstrbuf, sizeof(addrstrbuf))
			);
			break;
		}
		case AF_INET6: {
			struct sockaddr_in6 *in =
				(struct sockaddr_in6 *)&incoming;
			LOGF("AppAgent: Accepted connection from '%s'.",
			     inet_ntop(in->sin6_family, &in->sin6_addr,
				       addrstrbuf, sizeof(addrstrbuf))
			);
			break;
		}
		default:
			close(conn_fd);
			LOG("AppAgent: Unknown address family. Connection closed!");
			continue;
		}

		struct application_agent_comm_config *child_config = malloc(
			sizeof(struct application_agent_comm_config)
		);
		if (!child_config) {
			LOG("AppAgent: Error allocating memory for config!");
			close(conn_fd);
			continue;
		}
		child_config->parent = config;
		child_config->socket_fd = conn_fd;
		child_config->last_bundle_timestamp_ms = 0;
		child_config->last_bundle_sequence_number = 0;
		child_config->registered_agent_id = NULL;

		if (pipe(child_config->bundle_pipe_fd) == -1) {
			LOGERROR("AppAgent", "pipe()", errno);
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
			LOG("AppAgent: Error starting comm. task!");
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
		LOGERROR("AppAgent", "send()", wsp.errno_);
	return -wsp.errno_;
}

static void agent_msg_recv(struct bundle_adu data, void *param,
			   const void *bp_context)
{
	(void)bp_context;

	struct application_agent_comm_config *const config = (
		(struct application_agent_comm_config *)param
	);

	LOGF("AppAgent: Got Bundle for sink \"%s\" from \"%s\", forwarding.",
	     config->registered_agent_id, data.source);

	if (pipeq_write_all(config->bundle_pipe_fd[1],
			    &data, sizeof(struct bundle_adu)) <= 0) {
		LOGERROR("AppAgent", "write()", errno);
		bundle_adu_free_members(data);
	}
}

static int register_sink(char *sink_identifier,
	struct application_agent_comm_config *config)
{
	return bundle_processor_perform_agent_action(
		config->parent->bundle_agent_interface->bundle_signaling_queue,
		BP_SIGNAL_AGENT_REGISTER,
		sink_identifier,
		agent_msg_recv,
		config,
		true
	);
}

static void deregister_sink(struct application_agent_comm_config *config)
{
	if (config->registered_agent_id) {
		LOGF("AppAgent: De-registering agent ID \"%s\".",
		     config->registered_agent_id);

		bundle_processor_perform_agent_action(
			config->parent->bundle_agent_interface->bundle_signaling_queue,
			BP_SIGNAL_AGENT_DEREGISTER,
			config->registered_agent_id,
			NULL,
			NULL,
			true
		);

		free(config->registered_agent_id);
		config->registered_agent_id = NULL;
	}
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
		LOGF("AppAgent: Received registration request for ID \"%s\".",
		     msg.eid);

		deregister_sink(config);

		if (register_sink(msg.eid, config)) {
			response.type = AAP_MESSAGE_NACK;
		} else {
			config->registered_agent_id = msg.eid;
			msg.eid = NULL; // take over freeing of EID
			response.type = AAP_MESSAGE_ACK;
		}
		break;

	case AAP_MESSAGE_SENDBUNDLE:
	case AAP_MESSAGE_SENDBIBE:
		LOGF("AppAgent: Received %s (l = %zu) for %s via AAP.",
		     msg.type == AAP_MESSAGE_SENDBIBE ? "BIBE BPDU" : "bundle",
		     msg.payload_length, msg.eid);

		if (!config->registered_agent_id) {
			LOG("AppAgent: No agent ID registered, dropping!");
			break;
		}

		if (msg.type == AAP_MESSAGE_SENDBIBE) {
			LOG("AppAgent: ADU is a BPDU, prepending AR header!");

			#ifdef BIBE_CL_DRAFT_1_COMPATIBILITY
				uint8_t typecode = 7;
			#else
				uint8_t typecode = 3;
			#endif

			const size_t ar_size = msg.payload_length + 2;
			uint8_t *const ar_bytes = malloc(ar_size);

			memcpy(
				ar_bytes + 2,
				msg.payload,
				msg.payload_length
			);
			ar_bytes[0] = 0x82;     // CBOR array of length 2
			ar_bytes[1] = typecode; // Integer (record type)

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
			LOG("AppAgent: Bundle creation failed!");
			response.type = AAP_MESSAGE_NACK;
		} else {
			LOGF("AppAgent: Injected new bundle %p.", bundle);
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
		LOGF("AppAgent: Received bundle cancellation request for bundle #%llu.",
		     (uint64_t)msg.bundle_id);

		// TODO: Signal to Bundle Processor + implement handling of it
		LOG("AppAgent: Bundle cancellation failed (NOT IMPLEMENTED)!");

		response.type = AAP_MESSAGE_NACK;
		break;

	case AAP_MESSAGE_PING:
		LOGF(
			"AppAgent: Received PING from \"%s\"",
			config->registered_agent_id
			? config->registered_agent_id
			: "<not registered>"
		);
		response = (struct aap_message){ .type = AAP_MESSAGE_ACK };
		break;

	default:
		LOGF("AppAgent: Cannot handle AAP messages of type %d!",
		     msg.type);
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
			LOGERROR("AppAgent", "recv()", errno);
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
			LOG("AppAgent: Failed parsing received AAP message!");
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
			LOGERROR("AppAgent", "poll()", errno);
			// Try again if interrupted by a signal, else fail.
			if (errno == EINTR)
				continue;
			break;
		}
		if (pollfd[0].revents & POLLERR ||
		    pollfd[1].revents & POLLERR) {
			LOG("AppAgent: Socket error (e.g. TCP RST) detected.");
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
				LOGERROR("AppAgent", "read()", errno);
				break;
			}
			if (send_bundle(config->socket_fd, data) < 0)
				break;
		}
	}

	aap_parser_deinit(&parser);
done:
	close(config->bundle_pipe_fd[0]);
	close(config->bundle_pipe_fd[1]);
	deregister_sink(config);
	shutdown(config->socket_fd, SHUT_RDWR);
	close(config->socket_fd);
	free(config);
	LOG("AppAgent: Closed connection.");
}

static int create_unix_domain_socket(const char *path)
{
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sock == -1) {
		LOGERROR("AppAgent", "socket(AF_UNIX)", errno);
		return -1;
	}

	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	const int rv1 = snprintf(
		addr.sun_path,
		sizeof(addr.sun_path),
		"%s",
		path
	);

	if (rv1 <= 0 || (unsigned int)rv1 > sizeof(addr.sun_path)) {
		LOGF(
			"AppAgent: Invalid socket path, len = %d, maxlen = %zu",
			rv1,
			sizeof(addr.sun_path)
		);
		return -1;
	}

	unlink(path);
	int rv2 = bind(
		sock,
		(const struct sockaddr *)&addr,
		sizeof(struct sockaddr_un)
	);
	if (rv2 == -1) {
		LOGERROR("AppAgent", "bind(unix_domain_socket)", errno);
		return -1;
	}

	return sock;
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
		LOG("AppAgent: Error allocating memory for task config!");
		return NULL;
	}

	if (node && service)
		config->listen_socket =
			create_tcp_socket(node, service, false, NULL);
	else
		config->listen_socket =
			create_unix_domain_socket(socket_path);

	if (config->listen_socket < 0)  {
		LOG("AppAgent: Error binding to provided address!");
		free(config);
		return NULL;
	}

	if (listen(config->listen_socket, APPLICATION_AGENT_BACKLOG) < 0) {
		LOG("AppAgent: Error listening on provided address!");
		free(config);
		return NULL;
	}

	if (node && service)
		LOGF("AppAgent: Listening on [%s]:%s", node, service);
	else
		LOGF("AppAgent: Listening on %s", socket_path);

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
		LOG("AppAgent: Error creating listener task!");
		free(config);
		return NULL;
	}

	return config;
}
