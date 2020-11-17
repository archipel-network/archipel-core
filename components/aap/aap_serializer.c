#include "aap/aap.h"
#include "aap/aap_serializer.h"

#include "ud3tn/bundle.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void put_uint64(uint8_t *buffer, uint64_t value)
{
	buffer[0] = 0xFF & (value >> 56);
	buffer[1] = 0xFF & (value >> 48);
	buffer[2] = 0xFF & (value >> 40);
	buffer[3] = 0xFF & (value >> 32);
	buffer[4] = 0xFF & (value >> 24);
	buffer[5] = 0xFF & (value >> 16);
	buffer[6] = 0xFF & (value >> 8);
	buffer[7] = 0xFF & (value);
}

size_t aap_get_serialized_size(const struct aap_message *msg)
{
	size_t result = 1; // Only header

	// EID
	if (msg->type == AAP_MESSAGE_REGISTER ||
	    msg->type == AAP_MESSAGE_SENDBUNDLE ||
	    msg->type == AAP_MESSAGE_RECVBUNDLE ||
	    msg->type == AAP_MESSAGE_WELCOME) {
		result += 2;
		result += msg->eid_length;
	}

	// Payload
	if (msg->type == AAP_MESSAGE_SENDBUNDLE ||
	    msg->type == AAP_MESSAGE_RECVBUNDLE) {
		result += 8;
		result += msg->payload_length;
	}

	// Bundle ID
	if (msg->type == AAP_MESSAGE_SENDCONFIRM ||
	    msg->type == AAP_MESSAGE_CANCELBUNDLE)
		result += 8;

	return result;
}

void aap_serialize(const struct aap_message *msg,
	void (*write)(void *param, const void *data, const size_t length),
	void *param)
{
	uint8_t buffer[8];

	// Version + Type
	buffer[0] = (0x1 << 4) | (msg->type & 0xF);
	write(param, buffer, 1);

	// EID
	if (msg->type == AAP_MESSAGE_REGISTER ||
	    msg->type == AAP_MESSAGE_SENDBUNDLE ||
	    msg->type == AAP_MESSAGE_RECVBUNDLE ||
	    msg->type == AAP_MESSAGE_WELCOME) {
		buffer[0] = 0xFF & (msg->eid_length >> 8);
		buffer[1] = 0xFF & msg->eid_length;
		write(param, buffer, 2);
		if (msg->eid_length)
			write(param, msg->eid, msg->eid_length);
	}

	// Payload
	if (msg->type == AAP_MESSAGE_SENDBUNDLE ||
	    msg->type == AAP_MESSAGE_RECVBUNDLE) {
		put_uint64(buffer, msg->payload_length);
		write(param, buffer, 8);
		if (msg->payload_length)
			write(param, msg->payload, msg->payload_length);
	}

	// Bundle ID
	if (msg->type == AAP_MESSAGE_SENDCONFIRM ||
	    msg->type == AAP_MESSAGE_CANCELBUNDLE) {
		put_uint64(buffer, msg->bundle_id);
		write(param, buffer, 8);
	}
}

struct write_context {
	uint8_t *buffer;
	size_t position;
};

static void write_to_buffer(void *param, const void *data, const size_t length)
{
	struct write_context *ctx = (struct write_context *)param;

	memcpy(&ctx->buffer[ctx->position], data, length);
	ctx->position += length;
}

void aap_serialize_into(void *buffer, const struct aap_message *msg)
{
	struct write_context ctx = {.buffer = (uint8_t *)buffer, .position = 0};

	return aap_serialize(msg, write_to_buffer, &ctx);
}
