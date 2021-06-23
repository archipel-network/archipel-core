#include "aap/aap.h"
#include "aap/aap_parser.h"
#include "aap/aap_serializer.h"

#include "agents/application_agent.h"

#include "bundle6/create.h"
#include "bundle7/create.h"

#include "cla/posix/cla_tcp_util.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_task.h"
#include "platform/hal_types.h"

#include "platform/posix/pipe_queue_util.h"

#include "ud3tn/agent_manager.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/bundle.h"
#include "ud3tn/bundle_agent_interface.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/bundle_storage_manager.h"
#include "ud3tn/task_tags.h"

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
	uint64_t lifetime;

	int listen_socket;
	Task_t listener_task;
};

struct application_agent_comm_config {
	struct application_agent_config *parent;
	int socket_fd;
	int bundle_pipe_fd[2];
	Task_t task;
	char *registered_agent_id;
	uint64_t last_bundle_timestamp_s;
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
			LOGF("AppAgent: accept(): %s", strerror(errno));
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
		child_config->task = hal_task_create(
			application_agent_comm_task,
			"app_comm_t",
			APPLICATION_AGENT_TASK_PRIORITY,
			child_config,
			DEFAULT_TASK_STACK_SIZE,
			(void *)APPLICATION_AGENT_COMM_TASK_TAG
		);
		if (!child_config->task) {
			LOG("AppAgent: Error starting comm. task!");
			close(conn_fd);
			free(child_config);
		}
		child_config->last_bundle_timestamp_s = 0;
		child_config->last_bundle_sequence_number = 0;
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

struct write_socket_param {
	int socket_fd;
	int errno_;
};

static void write_to_socket(void *const p,
			    const void *const buffer, const size_t length)
{
	struct write_socket_param *const wsp = (struct write_socket_param *)p;
	ssize_t result;

	if (wsp->errno_)
		return;
	result = tcp_send_all(wsp->socket_fd, buffer, length);
	if (result == -1)
		wsp->errno_ = errno;
}

static int send_message(const int socket_fd,
			const struct aap_message *const msg)
{
	struct write_socket_param wsp = {
		.socket_fd = socket_fd,
		.errno_ = 0,
	};

	aap_serialize(msg, write_to_socket, &wsp);
	if (wsp.errno_)
		LOGF("send(): %s", strerror(wsp.errno_));
	return -wsp.errno_;
}

static void agent_msg_recv(struct bundle_adu data, void *param)
{
	struct application_agent_comm_config *const config = (
		(struct application_agent_comm_config *)param
	);

	LOGF("AppAgent: Got Bundle for sink \"%s\" from \"%s\", forwarding.",
	     config->registered_agent_id, data.source);

	if (pipeq_write_all(config->bundle_pipe_fd[1],
			    &data, sizeof(struct bundle_adu)) <= 0) {
		LOGF("AppAgent: write(): %s", strerror(errno));
		bundle_adu_free_members(data);
	}
}

static struct bundle *create_bundle(const uint8_t bp_version,
	const char *local_eid, char *sink_id, char *destination,
	const uint64_t creation_timestamp_s, const uint64_t sequence_number,
	const uint64_t lifetime, void *payload, size_t payload_length)
{
	const size_t local_eid_length = strlen(local_eid);
	const size_t sink_length = strlen(sink_id);
	char *source_eid = malloc(local_eid_length + sink_length + 2);

	if (source_eid == NULL) {
		free(payload);
		return NULL;
	}

	memcpy(source_eid, local_eid, local_eid_length);
	source_eid[local_eid_length] = '/';
	memcpy(&source_eid[local_eid_length + 1], sink_id, sink_length + 1);

	struct bundle *result;

	if (bp_version == 6)
		result = bundle6_create_local(
			payload, payload_length, source_eid, destination,
			creation_timestamp_s, sequence_number,
			lifetime, 0);
	else
		result = bundle7_create_local(
			payload, payload_length, source_eid, destination,
			creation_timestamp_s, sequence_number,
			lifetime, 0);

	free(source_eid);

	return result;
}

static bundleid_t create_forward_bundle(
	const struct bundle_agent_interface *bundle_agent_interface,
	const uint8_t bp_version, char *sink_id, char *destination,
	const uint64_t creation_timestamp_s, const uint64_t sequence_number,
	const uint64_t lifetime, void *payload, size_t payload_length)
{
	struct bundle *bundle = create_bundle(
		bp_version,
		bundle_agent_interface->local_eid,
		sink_id,
		destination,
		creation_timestamp_s,
		sequence_number,
		lifetime,
		payload,
		payload_length
	);

	if (bundle == NULL)
		return BUNDLE_INVALID_ID;

	bundleid_t bundle_id = bundle_storage_add(bundle);

	if (bundle_id != BUNDLE_INVALID_ID)
		bundle_processor_inform(
			bundle_agent_interface->bundle_signaling_queue,
			bundle_id,
			BP_SIGNAL_BUNDLE_LOCAL_DISPATCH,
			BUNDLE_SR_REASON_NO_INFO
		);
	else
		bundle_free(bundle);

	return bundle_id;
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
	const uint64_t time_s)
{
	if (config->last_bundle_timestamp_s == time_s)
		return ++config->last_bundle_sequence_number;

	config->last_bundle_timestamp_s = time_s;
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
		LOGF("AppAgent: Received bundle (l = %zu) for %s via AAP.",
		     msg.payload_length, msg.eid);

		if (!config->registered_agent_id) {
			LOG("AppAgent: No agent ID registered, dropping!");
			break;
		}

		const uint64_t time = hal_time_get_timestamp_s();
		const uint64_t seqnum = allocate_sequence_number(
			config,
			time
		);
		bundleid_t bundle_id = create_forward_bundle(
			config->parent->bundle_agent_interface,
			config->parent->bp_version,
			config->registered_agent_id,
			msg.eid,
			time,
			seqnum,
			config->parent->lifetime,
			msg.payload,
			msg.payload_length
		);
		// Pointer responsibility was taken by create_forward_bundle
		msg.payload = NULL;

		if (bundle_id == BUNDLE_INVALID_ID) {
			LOG("AppAgent: Bundle creation failed!");
			response.type = AAP_MESSAGE_NACK;
		} else {
			LOGF("AppAgent: Injected new bundle (#%llu).",
			     (uint64_t)bundle_id);
			response.type = AAP_MESSAGE_SENDCONFIRM;
			response.bundle_id = bundle_id;
		}

		break;

	case AAP_MESSAGE_CANCELBUNDLE:
		LOGF("Received bundle cancellation request for bundle #%llu.",
		     (uint64_t)msg.bundle_id);

		// TODO: Signal to Bundle Processor + implement handling of it
		LOG("Bundle cancellation failed (NOT IMPLEMENTED)!");

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
		MSG_DONTWAIT
	);
	if (recv_result <= 0) {
		if (recv_result < 0)
			LOGF("recv(): %s", strerror(errno));
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
		.type = AAP_MESSAGE_RECVBUNDLE,
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

	config->registered_agent_id = NULL;

	if (pipe(config->bundle_pipe_fd) == -1) {
		LOGF("AppAgent: pipe(): %s", strerror(errno));
		goto pipe_creation_error;
	}

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

	for (;;) {
		if (poll(pollfd, ARRAY_LENGTH(pollfd), -1) == -1) {
			LOGF("poll(): %s", strerror(errno));
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
				LOGF("read(): %s", strerror(errno));
				break;
			}
			if (send_bundle(config->socket_fd, data) < 0)
				break;
		}
	}

done:
	close(config->bundle_pipe_fd[0]);
	close(config->bundle_pipe_fd[1]);
pipe_creation_error:
	deregister_sink(config);
	shutdown(config->socket_fd, SHUT_RDWR);
	close(config->socket_fd);
	LOG("AppAgent: Closed connection.");
}

static int create_unix_domain_socket(const char *path)
{
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sock == -1) {
		LOGF("UNIX Domain Socket: Failed to create socket: %s",
		     errno ? strerror(errno) : "<unknown>");
		return -1;
	}

	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	unlink(path);
	int ret = bind(sock,
		       (const struct sockaddr *) &addr,
		       sizeof(struct sockaddr_un));
	if (ret == -1) {
		LOGF("UNIX Domain Socket: Failed to bind to: %s: %s",
		     addr.sun_path,
		     errno ? strerror(errno) : "<unknown>");
		return -1;
	}

	return sock;
}

struct application_agent_config *application_agent_setup(
	const struct bundle_agent_interface *bundle_agent_interface,
	const char *socket_path,
	const char *node, const char *service,
	const uint8_t bp_version, uint64_t lifetime)
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
	config->lifetime = lifetime;
	config->listener_task = hal_task_create(
		application_agent_listener_task,
		"app_listener_t",
		APPLICATION_AGENT_TASK_PRIORITY,
		config,
		DEFAULT_TASK_STACK_SIZE,
		(void *)APPLICATION_AGENT_LISTENER_TASK_TAG
	);
	if (!config->listener_task) {
		LOG("AppAgent: Error creating listener task!");
		free(config);
		return NULL;
	}
	return config;
}
