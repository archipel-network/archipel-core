// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

// Next version of the Application Agent Protocol, supporting forwarding modules
// and FIB control in extension to regular bundle reception/delivery.
// NOTE: This is still experimental - use with caution!

#include "aap2/aap2_agent.h"
#include "aap2/aap2.pb.h"

#include "bundle6/create.h"

#include "bundle7/create.h"

#include "cla/posix/cla_tcp_util.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"
#include "platform/hal_time.h"
#include "platform/hal_types.h"

#include "platform/posix/pipe_queue_util.h"

#include "ud3tn/common.h"
#include "ud3tn/bundle.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/eid.h"

#include <pb_decode.h>
#include <pb_encode.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>
#include <poll.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>

struct aap2_agent_config {
	const struct bundle_agent_interface *bundle_agent_interface;

	uint8_t bp_version;
	uint64_t lifetime_ms;

	int listen_socket;
};

struct aap2_agent_comm_config {
	struct aap2_agent_config *parent;
	int socket_fd;
	pb_istream_t pb_istream;

	int bundle_pipe_fd[2];

	bool is_subscriber;
	char *registered_eid;

	// TODO: Move to common multiplexer/demux state in BP. For now, allow
	// only one sender per ID.
	uint64_t last_bundle_timestamp_ms;
	uint64_t last_bundle_sequence_number;
};

// forward declaration
static void aap2_agent_comm_task(void *const param);

static bool pb_recv_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
	if (count == 0)
		return true;

	const int sock = (intptr_t)stream->state;
	ssize_t recvd = tcp_recv_all(sock, buf, count);

	// EOF?
	if (!recvd)
		stream->bytes_left = 0;
	else if (recvd < 0)
		LOGERROR("AAP2Agent", "recv()", errno);

	return (size_t)recvd == count;
}

static void aap2_agent_listener_task(void *const param)
{
	struct aap2_agent_config *const config = (
		(struct aap2_agent_config *)param
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
			LOGERROR("AAP2Agent", "accept()", errno);
			continue;
		}

		switch (incoming.ss_family) {
		case AF_UNIX:
			LOG("AAP2Agent: Accepted connection from UNIX Domain Socket.");
			break;
		case AF_INET: {
			struct sockaddr_in *in =
				(struct sockaddr_in *)&incoming;
			LOGF("AAP2Agent: Accepted connection from '%s'.",
			     inet_ntop(in->sin_family, &in->sin_addr,
				       addrstrbuf, sizeof(addrstrbuf))
			);
			break;
		}
		case AF_INET6: {
			struct sockaddr_in6 *in =
				(struct sockaddr_in6 *)&incoming;
			LOGF("AAP2Agent: Accepted connection from '%s'.",
			     inet_ntop(in->sin6_family, &in->sin6_addr,
				       addrstrbuf, sizeof(addrstrbuf))
			);
			break;
		}
		default:
			close(conn_fd);
			LOG("AAP2Agent: Unknown address family. Connection closed!");
			continue;
		}

		struct aap2_agent_comm_config *child_config = malloc(
			sizeof(struct aap2_agent_comm_config)
		);
		if (!child_config) {
			LOG("AAP2Agent: Error allocating memory for config!");
			close(conn_fd);
			continue;
		}
		child_config->parent = config;
		child_config->socket_fd = conn_fd;
		child_config->last_bundle_timestamp_ms = 0;
		child_config->last_bundle_sequence_number = 0;
		child_config->is_subscriber = false;
		child_config->registered_eid = NULL;

		child_config->pb_istream = (pb_istream_t){
			&pb_recv_callback,
			(void *)(intptr_t)conn_fd,
			SIZE_MAX,
			NULL,
		};

		if (pipe(child_config->bundle_pipe_fd) == -1) {
			LOGERROR("AAP2Agent", "pipe()", errno);
			close(conn_fd);
			free(child_config);
			continue;
		}

		const enum ud3tn_result task_creation_result = hal_task_create(
			aap2_agent_comm_task,
			"app_comm_t",
			AAP2_AGENT_TASK_PRIORITY,
			child_config,
			DEFAULT_TASK_STACK_SIZE
		);

		if (task_creation_result != UD3TN_OK) {
			LOG("AAP2Agent: Error starting comm. task!");
			close(conn_fd);
			free(child_config);
		}
	}
}

static bool write_callback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
	struct tcp_write_to_socket_param *const wsp = stream->state;

	tcp_write_to_socket(wsp, buf, count);

	return !wsp->errno_;
}

static int send_message(const int socket_fd,
			const pb_msgdesc_t *const fields,
			const void *const src_struct)
{
	struct tcp_write_to_socket_param wsp = {
		.socket_fd = socket_fd,
		.errno_ = 0,
	};

	pb_ostream_t stream = {
		&write_callback,
		&wsp,
		SIZE_MAX,
		0,
		NULL,
	};

	const bool ret = pb_encode_ex(
		&stream,
		fields,
		src_struct,
		PB_ENCODE_DELIMITED
	);

	if (wsp.errno_) {
		LOGERROR("AAP2Agent", "send()", wsp.errno_);
	} else if (!ret) {
		LOGF("AAP2Agent: Protobuf encode error: %s", PB_GET_ERROR(&stream));
		wsp.errno_ = EIO;
	}

	return -wsp.errno_;
}

static void agent_msg_recv(struct bundle_adu data, void *param,
			   const void *bp_context)
{
	(void)bp_context;

	struct aap2_agent_comm_config *const config = (
		(struct aap2_agent_comm_config *)param
	);

	LOGF("AAP2Agent: Got Bundle for EID \"%s\" from \"%s\", forwarding.",
	     config->registered_eid, data.source);

	if (pipeq_write_all(config->bundle_pipe_fd[1],
			    &data, sizeof(struct bundle_adu)) <= 0) {
		LOGERROR("AAP2Agent", "write()", errno);
		bundle_adu_free_members(data);
	}
}

static int register_sink(const char *sink_identifier, bool is_subscriber,
			 struct aap2_agent_comm_config *config)
{
	return bundle_processor_perform_agent_action(
		config->parent->bundle_agent_interface->bundle_signaling_queue,
		(
			is_subscriber
			? BP_SIGNAL_AGENT_REGISTER
			: BP_SIGNAL_AGENT_REGISTER_RPC
		),
		sink_identifier,
		is_subscriber ? agent_msg_recv : NULL,
		is_subscriber ? config : NULL,
		true
	);
}

static void deregister_sink(struct aap2_agent_comm_config *config)
{
	if (config->registered_eid) {
		const char *agent_id = get_agent_id_ptr(
			config->registered_eid
		);

		LOGF("AAP2Agent: De-registering agent ID \"%s\".",
			agent_id);

		ASSERT(bundle_processor_perform_agent_action(
			config->parent->bundle_agent_interface->bundle_signaling_queue,
			(
				config->is_subscriber
				? BP_SIGNAL_AGENT_DEREGISTER
				: BP_SIGNAL_AGENT_DEREGISTER_RPC
			),
			agent_id,
			NULL,
			NULL,
			true
		) == 0);

		free(config->registered_eid);
		config->registered_eid = NULL;
	}
}

static uint64_t allocate_sequence_number(
	struct aap2_agent_comm_config *const config,
	const uint64_t time_ms)
{
	if (config->last_bundle_timestamp_ms == time_ms)
		return ++config->last_bundle_sequence_number;

	config->last_bundle_timestamp_ms = time_ms;
	config->last_bundle_sequence_number = 1;

	return 1;
}

static aap2_ResponseStatus process_configure_msg(
	struct aap2_agent_comm_config *const config,
	aap2_ConnectionConfig * const msg)
{
	LOGF("AAP2Agent: Received request to %s for EID \"%s\".",
		msg->is_subscriber ? "subscribe" : "register",
		msg->endpoint_id);

	deregister_sink(config);

	if (validate_eid(msg->endpoint_id) != UD3TN_OK) {
		LOGF("AAP2Agent: Invalid EID: \"%s\"",
			msg->endpoint_id);
		return aap2_ResponseStatus_RESPONSE_STATUS_INVALID_REQUEST;
	}

	const char *sink_id = get_agent_id_ptr(
		msg->endpoint_id
	);

	if (!sink_id) {
		LOGF("AAP2Agent: Cannot obtain sink for EID: \"%s\"",
			msg->endpoint_id);
		return aap2_ResponseStatus_RESPONSE_STATUS_INVALID_REQUEST;
	}

	// TODO: Check authentication here!

	if (register_sink(sink_id, msg->is_subscriber,
				config)) {
		return aap2_ResponseStatus_RESPONSE_STATUS_ERROR;
	}

	config->registered_eid = msg->endpoint_id;
	msg->endpoint_id = NULL; // take over freeing

	config->is_subscriber = msg->is_subscriber;
	if (config->is_subscriber)
		LOG("AAP2Agent: Switching control flow!");

	return aap2_ResponseStatus_RESPONSE_STATUS_SUCCESS;
}

static aap2_ResponseStatus process_adu_msg(
	struct aap2_agent_comm_config *const config,
	aap2_BundleADU * const msg,
	uint8_t *payload_data,
	aap2_AAPResponse *response)
{
	LOGF("AAP2Agent: Received %s (l = %zu) for %s via AAP.",
		((msg->adu_flags &
		aap2_BundleADUFlags_BUNDLE_ADU_BPDU)
		? "BIBE BPDU" : "bundle"),
		msg->payload_length, msg->dst_eid);

	if (msg->creation_timestamp_ms != 0 || msg->sequence_number != 0) {
		LOG("AAP2Agent: User-defined creation timestamps are unsupported!");
		free(payload_data);
		return aap2_ResponseStatus_RESPONSE_STATUS_INVALID_REQUEST;
	}

	if (!config->registered_eid) {
		LOG("AAP2Agent: No agent ID registered, dropping!");
		free(payload_data);
		return aap2_ResponseStatus_RESPONSE_STATUS_NOT_FOUND;
	}

	if (!payload_data) {
		LOG("AAP2Agent: Cannot handle ADU without payload data!");
		return aap2_ResponseStatus_RESPONSE_STATUS_ERROR;
	}

	enum bundle_proc_flags flags = BUNDLE_FLAG_NONE;
	size_t payload_length = msg->payload_length;

	if (msg->adu_flags & aap2_BundleADUFlags_BUNDLE_ADU_BPDU) {
		LOG("AAP2Agent: ADU is a BPDU, prepending AR header!");

		#ifdef BIBE_CL_DRAFT_1_COMPATIBILITY
			const uint8_t typecode = 7;
		#else
			const uint8_t typecode = 3;
		#endif

		const size_t ar_size = payload_length + 2;
		uint8_t *const ar_bytes = malloc(ar_size);

		memcpy(
			ar_bytes + 2,
			payload_data,
			payload_length
		);
		ar_bytes[0] = 0x82;     // CBOR array of length 2
		ar_bytes[1] = typecode; // Integer (record type)

		free(payload_data);
		payload_data = ar_bytes;
		payload_length = ar_size;
		flags |= BUNDLE_FLAG_ADMINISTRATIVE_RECORD;
	}

	const uint64_t time_ms = hal_time_get_timestamp_ms();
	const uint64_t seqnum = allocate_sequence_number(
		config,
		time_ms
	);

	struct bundle *bundle;

	if (config->parent->bp_version == 6)
		bundle = bundle6_create_local(
			payload_data,
			payload_length,
			config->registered_eid,
			msg->dst_eid,
			time_ms, seqnum,
			config->parent->lifetime_ms,
			flags
		);
	else
		bundle = bundle7_create_local(
			payload_data,
			payload_length,
			config->registered_eid,
			msg->dst_eid,
			time_ms,
			seqnum,
			config->parent->lifetime_ms,
			flags
		);

	if (!bundle) {
		LOG("AAP2Agent: Bundle creation failed!");
		return aap2_ResponseStatus_RESPONSE_STATUS_ERROR;
	}

	bundle_processor_inform(
		config->parent->bundle_agent_interface->bundle_signaling_queue,
		bundle,
		BP_SIGNAL_BUNDLE_LOCAL_DISPATCH,
		NULL,
		NULL,
		NULL,
		NULL
	);

	LOGF("AAP2Agent: Injected new bundle %p.", bundle);

	response->bundle_headers.src_eid = strdup(config->registered_eid);
	response->bundle_headers.dst_eid = msg->dst_eid;
	msg->dst_eid = NULL;
	response->bundle_headers.payload_length = payload_length;
	response->bundle_headers.creation_timestamp_ms = time_ms;
	response->bundle_headers.sequence_number = seqnum;
	response->bundle_headers.lifetime_ms = config->parent->lifetime_ms;

	return aap2_ResponseStatus_RESPONSE_STATUS_SUCCESS;
}

static int process_aap_message(
	struct aap2_agent_comm_config *const config,
	aap2_AAPMessage * const msg,
	uint8_t *const payload_data)
{
	aap2_AAPResponse response = aap2_AAPResponse_init_default;
	int result = -1;

	// Default response
	response.response_status = (
		aap2_ResponseStatus_RESPONSE_STATUS_INVALID_REQUEST
	);

	switch (msg->which_msg) {
	case aap2_AAPMessage_config_tag:
		response.response_status = process_configure_msg(
			config,
			&msg->msg.config
		);
		break;
	case aap2_AAPMessage_adu_tag:
		response.response_status = process_adu_msg(
			config,
			&msg->msg.adu,
			payload_data,
			&response
		);
		break;
	case aap2_AAPMessage_keepalive_tag:
		LOGF(
			"AAP2Agent: Received KEEPALIVE from \"%s\"",
			config->registered_eid
			? config->registered_eid
			: "<not registered>"
		);
		response.response_status = (
			aap2_ResponseStatus_RESPONSE_STATUS_ACK
		);
		break;
	default:
		LOGF("AAP2Agent: Cannot handle AAP messages of tag type %d!",
		     msg->which_msg);
		break;
	}

	result = send_message(
		config->socket_fd,
		aap2_AAPResponse_fields,
		&response
	);

	// Free message contents
	pb_release(aap2_AAPResponse_fields, &response);
	return result;
}

static bool poll_recv_timeout(const int socket_fd, const int timeout)
{
	struct pollfd pollfd[1];

	pollfd[0].events = POLLIN;
	pollfd[0].fd = socket_fd;

	for (;;) {
		if (poll(pollfd, ARRAY_LENGTH(pollfd), timeout) == -1) {
			const int err = errno;

			// Try again if interrupted by a signal.
			if (err == EINTR)
				continue;

			LOGERROR("AAP2Agent", "poll()", err);
			return false;
		}
		if ((pollfd[0].revents & POLLERR)) {
			LOG("AAP2Agent: Socket error (e.g. TCP RST) detected.");
			return false;
		}
		if (pollfd[0].revents & POLLHUP) {
			LOG("AAP2Agent: The peer closed the connection.");
			return false;
		}
		if (pollfd[0].revents & POLLIN)
			return true;
		return false;
	}
}

// NOTE: Callback-style serialization would be use a function as follows:
// static bool write_string(pb_ostream_t *stream, const pb_field_iter_t *field,
//                          void *const *arg)
// {
//         const char *str = *arg;
//
//         if (!pb_encode_tag_for_field(stream, field))
//                 return false;
//
//         return pb_encode_string(stream, (uint8_t *)str, strlen(str));
// }

static int send_bundle_from_pipe(struct aap2_agent_comm_config *const config)
{
	struct bundle_adu data;

	if (pipeq_read_all(config->bundle_pipe_fd[0],
			   &data, sizeof(struct bundle_adu)) <= 0) {
		LOGERROR("AAP2Agent", "read()", errno);
		return -1;
	}

	aap2_AAPMessage msg = aap2_AAPMessage_init_default;

	msg.which_msg = aap2_AAPMessage_adu_tag;
	// NOTE: Callback-style serialization would be as follows:
	// msg.msg.adu.dst_eid.funcs.encode = &write_string;
	// msg.msg.adu.dst_eid.arg = data.destination;
	msg.msg.adu.dst_eid = data.destination;
	msg.msg.adu.src_eid = data.source;
	msg.msg.adu.payload_length = data.length;
	msg.msg.adu.creation_timestamp_ms = data.bundle_creation_timestamp_ms;
	msg.msg.adu.sequence_number = data.bundle_sequence_number;
	// BIBE
	if (data.proc_flags == BUNDLE_FLAG_ADMINISTRATIVE_RECORD)
		msg.msg.adu.adu_flags = aap2_BundleADUFlags_BUNDLE_ADU_BPDU;

	const int socket_fd = config->socket_fd;
	int send_result = send_message(
		socket_fd,
		aap2_AAPMessage_fields,
		&msg
	);

	// NOTE: We do not "release" msg as there is nothing to deallocate here.

	if (send_result < 0) {
		bundle_adu_free_members(data);
		return send_result;
	}

	struct tcp_write_to_socket_param wsp = {
		.socket_fd = socket_fd,
		.errno_ = 0,
	};
	pb_ostream_t stream = {
		&write_callback,
		&wsp,
		SIZE_MAX,
		0,
		NULL,
	};

	const bool ret = pb_write(&stream, data.payload, data.length);

	if (wsp.errno_) {
		LOGERROR("AAP2Agent", "send()", wsp.errno_);
		send_result = -1;
	} else if (!ret) {
		LOGF("AAP2Agent: pb_write() error: %s",
			PB_GET_ERROR(&stream));
		send_result = -1;
	}

	if (send_result < 0) {
		bundle_adu_free_members(data);
		return send_result;
	}

	// Receive status from client.
	aap2_AAPResponse response = aap2_AAPResponse_init_default;

	if (!poll_recv_timeout(config->socket_fd, AAP2_AGENT_TIMEOUT_MS)) {
		LOG("AAP2Agent: Client did not respond, closing connection.");
		send_result = -1;
		goto done;
	}

	const bool success = pb_decode_ex(
		&config->pb_istream,
		aap2_AAPResponse_fields,
		&response,
		PB_DECODE_DELIMITED
	);

	if (!success) {
		LOGF("AAP2Agent: Protobuf decode error: %s",
		     PB_GET_ERROR(&config->pb_istream));
		send_result = -1;
		goto done;
	}

	if (response.response_status !=
	    aap2_ResponseStatus_RESPONSE_STATUS_SUCCESS) {
		// TODO: Implement configurable policy to re-route/keep somehow.
		LOG("AAP2Agent: Client reported error for bundle, dropping.");
		// NOTE: Do not return -1 here as this would close the socket.
	}

done:
	pb_release(aap2_AAPResponse_fields, &response);
	bundle_adu_free_members(data);
	return send_result;
}

static void shutdown_bundle_pipe(int bundle_pipe_fd[2])
{
	struct bundle_adu data;

	while (poll_recv_timeout(bundle_pipe_fd[0], 0)) {
		if (pipeq_read_all(bundle_pipe_fd[0],
				   &data, sizeof(struct bundle_adu)) <= 0) {
			LOGERROR("AAP2Agent", "read()", errno);
			break;
		}

		LOGF("AAP2Agent: Dropping unsent bundle from '%s'.",
		     data.source);
		bundle_adu_free_members(data);
	}

	close(bundle_pipe_fd[0]);
	close(bundle_pipe_fd[1]);
}

static void aap2_agent_comm_task(void *const param)
{
	struct aap2_agent_comm_config *const config = (
		(struct aap2_agent_comm_config *)param
	);

	// Send indicator for invalid version.
	const uint8_t aap2_version_indicator = 0x2F;

	if (tcp_send_all(config->socket_fd, &aap2_version_indicator, 1) != 1) {
		LOGERROR("AAP2Agent", "send()", errno);
		goto done;
	}

	char *local_eid = config->parent->bundle_agent_interface->local_eid;
	aap2_AAPMessage msg = aap2_AAPMessage_init_default;

	msg.which_msg = aap2_AAPMessage_welcome_tag;
	msg.msg.welcome.node_id = local_eid;

	if (send_message(config->socket_fd, aap2_AAPMessage_fields, &msg))
		goto done;
	// NOTE: We do not "release" msg as there is nothing to deallocate here.

	aap2_AAPMessage request;
	struct pollfd pollfd[2];

	pollfd[0].events = POLLIN;
	pollfd[0].fd = config->socket_fd;
	pollfd[1].events = POLLIN;
	pollfd[1].fd = config->bundle_pipe_fd[0];

	for (;;) {
		if (config->is_subscriber) {
			// TODO: Timeout & send keepalive messages!
			if (poll(pollfd, ARRAY_LENGTH(pollfd), -1) == -1) {
				const int err = errno;

				LOGERROR("AAP2Agent", "poll()", err);
				// Try again if interrupted by a signal.
				if (err == EINTR)
					continue;
				break;
			}
			if ((pollfd[0].revents & POLLERR) ||
			    (pollfd[1].revents & POLLERR)) {
				LOG("AAP2Agent: Socket error (e.g. TCP RST) detected.");
				break;
			}
			if (pollfd[0].revents & POLLHUP) {
				LOG("AAP2Agent: The peer closed the connection.");
				break;
			}
			if (pollfd[0].revents & POLLIN) {
				LOG("AAP2Agent: Unexpected data on socket, terminating.");
				break;
			}
			if (pollfd[1].revents & POLLIN)
				if (send_bundle_from_pipe(config) < 0)
					break;
		} else {
			// TODO: poll() beforehand to detect closure of socket
			// so that NanoPB does not return "io error".
			bool success = pb_decode_ex(
				&config->pb_istream,
				aap2_AAPMessage_fields,
				&request,
				PB_DECODE_DELIMITED
			);

			if (!success) {
				LOGF("AAP2Agent: Protobuf decode error: %s",
				     PB_GET_ERROR(&config->pb_istream));
				break;
			}

			uint8_t *payload = NULL;

			// Read payload for ADU messages. Note that even if this
			// fails we process the message with payload == NULL, to
			// send a proper response.
			if (request.which_msg == aap2_AAPMessage_adu_tag) {
				if (request.msg.adu.payload_length <=
				    BUNDLE_MAX_SIZE) {
					payload = malloc(
						request.msg.adu.payload_length
					);
					if (!payload) {
						LOG("AAP2Agent: Payload alloc error!");
					} else {
						success = pb_read(
							&config->pb_istream,
							payload,
							request.msg.adu.
								payload_length
						);
						if (!success) {
							free(payload);
							payload = NULL;
							LOG("AAP2Agent: Payload read error!");
						}
					}
				} else {
					LOG("AAP2Agent: Payload too large!");
				}
				if (!payload) {
					pb_release(
						aap2_AAPMessage_fields,
						&request
					);
					break;
				}
			}

			success = !process_aap_message(
				config,
				&request,
				payload
			);

			pb_release(aap2_AAPMessage_fields, &request);
			if (!success)
				break;
		}
	}

done:
	deregister_sink(config);
	shutdown_bundle_pipe(config->bundle_pipe_fd);
	shutdown(config->socket_fd, SHUT_RDWR);
	close(config->socket_fd);
	free(config);
	LOG("AAP2Agent: Closed connection.");
}

static int create_unix_domain_socket(const char *path)
{
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sock == -1) {
		LOGERROR("AAP2Agent", "socket(AF_UNIX)", errno);
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
			"AAP2Agent: Invalid socket path, len = %d, maxlen = %zu",
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
		LOGERROR("AAP2Agent", "bind(unix_domain_socket)", errno);
		return -1;
	}

	return sock;
}

struct aap2_agent_config *aap2_agent_setup(
	const struct bundle_agent_interface *bundle_agent_interface,
	const char *socket_path,
	const char *node, const char *service,
	const uint8_t bp_version, uint64_t lifetime_ms)
{
	struct aap2_agent_config *const config = malloc(
		sizeof(struct aap2_agent_config)
	);

	if (!config) {
		LOG("AAP2Agent: Error allocating memory for task config!");
		return NULL;
	}

	if (node && service) {
		config->listen_socket =
			create_tcp_socket(node, service, false, NULL);
	} else if (socket_path) {
		config->listen_socket =
			create_unix_domain_socket(socket_path);
	} else {
		LOG("AAP2Agent: Invalid socket provided!");
		free(config);
		return NULL;
	}

	if (config->listen_socket < 0)  {
		LOG("AAP2Agent: Error binding to provided address!");
		free(config);
		return NULL;
	}

	if (listen(config->listen_socket, AAP2_AGENT_BACKLOG) < 0) {
		LOG("AAP2Agent: Error listening on provided address!");
		free(config);
		return NULL;
	}

	if (node && service)
		LOGF("AAP2Agent: Listening on [%s]:%s", node, service);
	else
		LOGF("AAP2Agent: Listening on %s", socket_path);

	config->bundle_agent_interface = bundle_agent_interface;
	config->bp_version = bp_version;
	config->lifetime_ms = lifetime_ms;

	const enum ud3tn_result task_creation_result = hal_task_create(
		aap2_agent_listener_task,
		"app_listener_t",
		AAP2_AGENT_TASK_PRIORITY,
		config,
		DEFAULT_TASK_STACK_SIZE
	);

	if (task_creation_result != UD3TN_OK) {
		LOG("AAP2Agent: Error creating listener task!");
		free(config);
		return NULL;
	}

	return config;
}
