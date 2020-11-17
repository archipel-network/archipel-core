#include "aap/aap.h"
#include "aap/aap_parser.h"

#include "ud3tn/bundle.h"
#include "ud3tn/common.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static size_t aap_parse_type(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length);
static size_t aap_parse_eid_length(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length);
static size_t aap_parse_eid(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length);
static size_t aap_parse_payload_length(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length);
static size_t aap_parse_payload(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length);
static size_t aap_parse_bundle_id(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length);


static uint64_t read_uint64(const uint8_t *buffer)
{
	return (
		(uint64_t)buffer[0] << 56 | (uint64_t)buffer[1] << 48 |
		(uint64_t)buffer[2] << 40 | (uint64_t)buffer[3] << 32 |
		(uint64_t)buffer[4] << 24 | (uint64_t)buffer[5] << 16 |
		(uint64_t)buffer[6] <<  8 | (uint64_t)buffer[7]
	);
}


static size_t aap_parse_type(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length)
{
	if (length == 0)
		return 0;

	// Protocol version
	if (((buffer[0] & 0xF0) >> 4) != 0x1) {
		parser->status = PARSER_STATUS_ERROR;
		return 1;
	}
	parser->message.type = (enum aap_message_type)(buffer[0] & 0x0F);

	switch (parser->message.type) {
	case AAP_MESSAGE_ACK:
	case AAP_MESSAGE_NACK:
	case AAP_MESSAGE_PING:
		parser->status = PARSER_STATUS_DONE;
		break;
	case AAP_MESSAGE_REGISTER:
	case AAP_MESSAGE_SENDBUNDLE:
	case AAP_MESSAGE_RECVBUNDLE:
	case AAP_MESSAGE_WELCOME:
		parser->parse = aap_parse_eid_length;
		break;
	case AAP_MESSAGE_SENDCONFIRM:
	case AAP_MESSAGE_CANCELBUNDLE:
		parser->parse = aap_parse_bundle_id;
		break;
	default:
		parser->status = PARSER_STATUS_ERROR;
		break;
	}

	return 1;
}

static size_t aap_parse_eid_length(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length)
{
	if (length < 2)
		return 0;

	parser->message.eid_length = (size_t)(
		(uint16_t)buffer[0] << 8 | (uint16_t)buffer[1]
	);
	parser->consumed = 0;
	parser->remaining = parser->message.eid_length;

	parser->message.eid = malloc(parser->message.eid_length + 1);
	parser->message.eid[parser->message.eid_length] = '\0';
	if (parser->message.eid) {
		if (!parser->message.eid_length)
			parser->status = PARSER_STATUS_DONE;
		parser->parse = aap_parse_eid;
	} else {
		parser->status = PARSER_STATUS_ERROR;
	}

	return 2;
}

static size_t aap_parse_eid(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length)
{
	const size_t consumed = MIN(length, parser->remaining);

	memcpy(&parser->message.eid[parser->consumed], buffer, consumed);
	parser->consumed += consumed;
	parser->remaining -= consumed;

	if (!parser->remaining) {
		parser->message.eid[parser->message.eid_length] = '\0';

		switch (parser->message.type) {
		case AAP_MESSAGE_REGISTER:
		case AAP_MESSAGE_WELCOME:
			parser->status = PARSER_STATUS_DONE;
			break;
		case AAP_MESSAGE_SENDBUNDLE:
		case AAP_MESSAGE_RECVBUNDLE:
			parser->parse = aap_parse_payload_length;
			break;
		default:
			// Invalid type. We should never get here.
			abort();
		}
	}

	return consumed;
}

static size_t aap_parse_payload_length(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length)
{
	if (length < 8)
		return 0;

	const uint64_t payload_length = read_uint64(buffer);

	// On some platforms (e.g. STM32), size_t is smaller than uint64_t.
	if (payload_length > (uint64_t)SIZE_MAX) {
		parser->status = PARSER_STATUS_ERROR;
		return 8;
	}
	parser->message.payload_length = (size_t)payload_length;
	parser->consumed = 0;
	parser->remaining = parser->message.payload_length;
	if (parser->max_payload_length &&
	    parser->message.payload_length > parser->max_payload_length) {
		parser->status = PARSER_STATUS_ERROR;
		return 8;
	}

	parser->message.payload = malloc(parser->message.payload_length);
	if (parser->message.payload) {
		if (!parser->message.payload_length)
			parser->status = PARSER_STATUS_DONE;
		parser->parse = aap_parse_payload;
	} else {
		parser->status = PARSER_STATUS_ERROR;
	}

	return 8;
}

static size_t aap_parse_payload(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length)
{
	const size_t consumed = MIN(length, parser->remaining);

	memcpy(&parser->message.payload[parser->consumed], buffer, consumed);
	parser->consumed += consumed;
	parser->remaining -= consumed;

	if (!parser->remaining)
		parser->status = PARSER_STATUS_DONE;

	return consumed;
}

static size_t aap_parse_bundle_id(
	struct aap_parser *const parser,
	const uint8_t *const buffer, const size_t length)
{
	if (length < 8)
		return 0;

	parser->message.bundle_id = (bundleid_t)read_uint64(buffer);

	parser->status = PARSER_STATUS_DONE;
	return 8;
}


void aap_parser_init(struct aap_parser *parser)
{
	parser->max_payload_length = 0;
	parser->parse = NULL;
	memset(&parser->message, 0, sizeof(struct aap_message));
	aap_parser_reset(parser);
}

void aap_parser_reset(struct aap_parser *parser)
{
	parser->parse = &aap_parse_type;
	parser->status = PARSER_STATUS_GOOD;
	aap_message_clear(&parser->message);
}

struct aap_message aap_parser_extract_message(struct aap_parser *parser)
{
	struct aap_message message = parser->message;

	if (parser->status != PARSER_STATUS_DONE)
		message.type = AAP_MESSAGE_INVALID;
	memset(&parser->message, 0, sizeof(struct aap_message));
	aap_message_clear(&parser->message);
	return message;
}
