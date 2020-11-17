#include "aap/aap.h"
#include "aap/aap_parser.h"

#include "ud3tn/common.h"

#include "unity_fixture.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MAX_PARSE_METHOD_CALLS 100
#define MAX_PAYLOAD_LENGTH_DEFAULT 10

TEST_GROUP(aap_parser);

static struct aap_parser parser;

TEST_SETUP(aap_parser)
{
	aap_parser_init(&parser);
	parser.max_payload_length = MAX_PAYLOAD_LENGTH_DEFAULT;
}

TEST_TEAR_DOWN(aap_parser)
{
	aap_parser_reset(&parser);
}

/*
 * This calls the parser in a loop as long as...
 * 1) there are bytes in the buffer,
 * 2) it is not done or has encountered an error, and
 * 3) it has consumed at least one byte in the last iteration, i.e. the
 *    remaining buffer contents are sufficient to continue parsing.
 *
 * It is expected that an implementation using the parser are
 * realized in the same manner.
 */
static size_t parse(const uint8_t *const input, const size_t length)
{
	size_t delta = 0;
	size_t consumed = 0;
	int calls = 0;

	while (parser.status == PARSER_STATUS_GOOD && consumed < length) {
		TEST_ASSERT_MESSAGE(++calls < MAX_PARSE_METHOD_CALLS,
				    "endless loop detected");
		delta = parser.parse(
			&parser,
			input + consumed,
			length - consumed
		);
		if (!delta)
			break;
		consumed += delta;
	}

	return consumed;
}

TEST(aap_parser, init_and_reset)
{
	aap_parser_init(&parser);

	// After initialization
	void *parse_method = parser.parse;

	TEST_ASSERT_NOT_NULL(parse_method);
	TEST_ASSERT(!aap_message_is_valid(&parser.message));
	TEST_ASSERT_NULL(parser.message.eid);
	TEST_ASSERT_EQUAL(0, parser.max_payload_length);
	parser.max_payload_length = 1024;
	aap_parser_reset(&parser);
	TEST_ASSERT_NOT_NULL(parser.parse);
	TEST_ASSERT_EQUAL_PTR(parse_method, parser.parse);
	TEST_ASSERT(!aap_message_is_valid(&parser.message));
	TEST_ASSERT_NULL(parser.message.payload);
	TEST_ASSERT_EQUAL(1024, parser.max_payload_length);
}

TEST(aap_parser, parse_ack_message)
{
	const uint8_t msg_ack[] = {
		0x10,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(1, parse(msg_ack,
				   ARRAY_SIZE(msg_ack)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_ACK, parser.message.type);
	TEST_ASSERT_NULL(parser.message.eid);
	TEST_ASSERT_NULL(parser.message.payload);
	TEST_ASSERT(aap_message_is_valid(&parser.message));

	const uint8_t msg_ack_trailing_bytes[] = {
		0x10, 0x42, 0x42,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(1, parse(msg_ack_trailing_bytes,
				   ARRAY_SIZE(msg_ack_trailing_bytes)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_ACK, parser.message.type);
	TEST_ASSERT_NULL(parser.message.eid);
	TEST_ASSERT_NULL(parser.message.payload);
	TEST_ASSERT(aap_message_is_valid(&parser.message));
}

TEST(aap_parser, parse_nack_message)
{
	const uint8_t msg_nack[] = {
		0x11,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(1, parse(msg_nack,
				   ARRAY_SIZE(msg_nack)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_NACK, parser.message.type);
	TEST_ASSERT_NULL(parser.message.eid);
	TEST_ASSERT_NULL(parser.message.payload);
	TEST_ASSERT(aap_message_is_valid(&parser.message));

	const uint8_t msg_nack_trailing_bytes[] = {
		0x11, 0x42, 0x42,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(1, parse(msg_nack_trailing_bytes,
				   ARRAY_SIZE(msg_nack_trailing_bytes)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_NACK, parser.message.type);
	TEST_ASSERT_NULL(parser.message.eid);
	TEST_ASSERT_NULL(parser.message.payload);
	TEST_ASSERT(aap_message_is_valid(&parser.message));
}

TEST(aap_parser, parse_register_message)
{
	const uint8_t msg_register[] = {
		0x12, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_register),
			  parse(msg_register, ARRAY_SIZE(msg_register)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_REGISTER, parser.message.type);
	TEST_ASSERT_EQUAL(5, parser.message.eid_length);
	TEST_ASSERT_NOT_NULL(parser.message.eid);
	TEST_ASSERT(parser.message.eid[5] == '\0');
	TEST_ASSERT_EQUAL_STRING("UD3TN", parser.message.eid);
	TEST_ASSERT(aap_message_is_valid(&parser.message));
}

TEST(aap_parser, parse_sendbundle_message)
{
	const uint8_t msg_sendbundle[] = {
		0x13, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
		'P', 'A', 'Y', 'L', 'O', 'A', 'D',
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_sendbundle),
			  parse(msg_sendbundle, ARRAY_SIZE(msg_sendbundle)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_SENDBUNDLE, parser.message.type);
	TEST_ASSERT_EQUAL(5, parser.message.eid_length);
	TEST_ASSERT(parser.message.eid[5] == '\0');
	TEST_ASSERT_EQUAL_STRING("UD3TN", parser.message.eid);
	TEST_ASSERT_EQUAL(7, parser.message.payload_length);
	TEST_ASSERT_EQUAL_MEMORY("PAYLOAD", parser.message.payload,
				 parser.message.payload_length);
	TEST_ASSERT(aap_message_is_valid(&parser.message));

	const size_t msg_size_without_payload = 16;
	const size_t payload_size = 1000;
	uint8_t msg_sendbundle_big[16 + 1000] = {
		0x13, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE8,
	};
	memset(msg_sendbundle_big + msg_size_without_payload, 42, payload_size);

	parser.max_payload_length = payload_size - 1;
	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(msg_size_without_payload,
			  parse(msg_sendbundle_big,
				ARRAY_SIZE(msg_sendbundle_big)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_ERROR, parser.status);

	parser.max_payload_length = payload_size;
	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_sendbundle_big),
			  parse(msg_sendbundle_big,
				ARRAY_SIZE(msg_sendbundle_big)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_SENDBUNDLE, parser.message.type);
	TEST_ASSERT_EQUAL(5, parser.message.eid_length);
	TEST_ASSERT(parser.message.eid[5] == '\0');
	TEST_ASSERT_EQUAL_STRING("UD3TN", parser.message.eid);
	TEST_ASSERT_EQUAL(payload_size, parser.message.payload_length);
	TEST_ASSERT_EQUAL_MEMORY(msg_sendbundle_big + msg_size_without_payload,
				 parser.message.payload,
				 parser.message.payload_length);
	TEST_ASSERT(aap_message_is_valid(&parser.message));

	uint8_t msg_sendbundle_huge[] = {
		0x13, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	};
	parser.max_payload_length = 2UL * 1024 * 1024 * 1024; // maybe realistic
	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_sendbundle_huge),
			  parse(msg_sendbundle_huge,
				SIZE_MAX));
	TEST_ASSERT_EQUAL(PARSER_STATUS_ERROR, parser.status);
}

TEST(aap_parser, parse_recvbundle_message)
{
	const uint8_t msg_recvbundle[] = {
		0x14, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
		'P', 'A', 'Y', 'L', 'O', 'A', 'D',
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_recvbundle),
			  parse(msg_recvbundle, ARRAY_SIZE(msg_recvbundle)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_RECVBUNDLE, parser.message.type);
	TEST_ASSERT_EQUAL(5, parser.message.eid_length);
	TEST_ASSERT(parser.message.eid[5] == '\0');
	TEST_ASSERT_EQUAL_STRING("UD3TN", parser.message.eid);
	TEST_ASSERT_EQUAL(7, parser.message.payload_length);
	TEST_ASSERT_EQUAL_MEMORY("PAYLOAD", parser.message.payload,
				 parser.message.payload_length);
	TEST_ASSERT(aap_message_is_valid(&parser.message));
}

TEST(aap_parser, parse_sendconfirm_message)
{
	const uint8_t msg_sendconfirm[] = {
		0x15,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x35, 0x17,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_sendconfirm),
			  parse(msg_sendconfirm, ARRAY_SIZE(msg_sendconfirm)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_SENDCONFIRM, parser.message.type);
	TEST_ASSERT_NULL(parser.message.eid);
	TEST_ASSERT_NULL(parser.message.payload);
	TEST_ASSERT_EQUAL(0x3517, parser.message.bundle_id);
	TEST_ASSERT(aap_message_is_valid(&parser.message));
}

TEST(aap_parser, parse_cancelbundle_message)
{
	const uint8_t msg_cancelbundle[] = {
		0x16,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x35,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_cancelbundle),
			  parse(msg_cancelbundle,
				ARRAY_SIZE(msg_cancelbundle)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_CANCELBUNDLE, parser.message.type);
	TEST_ASSERT_NULL(parser.message.eid);
	TEST_ASSERT_NULL(parser.message.payload);
	TEST_ASSERT_EQUAL(0x1735, parser.message.bundle_id);
	TEST_ASSERT(aap_message_is_valid(&parser.message));
}

TEST(aap_parser, parse_welcome_message)
{
	const uint8_t msg_welcome[] = {
		0x17, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_welcome),
			  parse(msg_welcome, ARRAY_SIZE(msg_welcome)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_WELCOME, parser.message.type);
	TEST_ASSERT_EQUAL(5, parser.message.eid_length);
	TEST_ASSERT_NOT_NULL(parser.message.eid);
	TEST_ASSERT(parser.message.eid[5] == '\0');
	TEST_ASSERT_EQUAL_STRING("UD3TN", parser.message.eid);
	TEST_ASSERT_NULL(parser.message.payload);
	TEST_ASSERT(aap_message_is_valid(&parser.message));

	const uint8_t msg_welcome_with_null[] = {
		0x17, 0x00, 0x0A,
		'U', 'D', '3', 'T', 'N', ' ', '\0', ' ', '4', '2',
		0x42,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_welcome_with_null) - 1,
			  parse(msg_welcome_with_null,
				ARRAY_SIZE(msg_welcome_with_null)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_WELCOME, parser.message.type);
	TEST_ASSERT_EQUAL(10, parser.message.eid_length);
	TEST_ASSERT_NOT_NULL(parser.message.eid);
	TEST_ASSERT_EQUAL_MEMORY("UD3TN \0 42\0", parser.message.eid,
				 parser.message.eid_length + 1);
	TEST_ASSERT_NULL(parser.message.payload);
	// The message should be parsed successfully, but is invalid for uD3TN.
	TEST_ASSERT(!aap_message_is_valid(&parser.message));
}

TEST(aap_parser, parse_ping_message)
{
	const uint8_t msg_ping_trailing_bytes[] = {
		0x18, 0x42, 0x42, 0x42,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(1, parse(msg_ping_trailing_bytes,
				   ARRAY_SIZE(msg_ping_trailing_bytes)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_PING, parser.message.type);
	TEST_ASSERT_NULL(parser.message.eid);
	TEST_ASSERT_NULL(parser.message.payload);
	TEST_ASSERT(aap_message_is_valid(&parser.message));
}

TEST(aap_parser, parse_invalid_message)
{
	const uint8_t msg_invalid_version[] = {
		0x22,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(1, parse(msg_invalid_version,
				   ARRAY_SIZE(msg_invalid_version)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_ERROR, parser.status);

	const uint8_t msg_invalid_type[] = {
		0x1F,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(1, parse(msg_invalid_type,
				   ARRAY_SIZE(msg_invalid_type)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_ERROR, parser.status);

	const uint8_t msg_invalid_type_and_version[] = {
		0x6F,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(1, parse(msg_invalid_type_and_version,
				   ARRAY_SIZE(msg_invalid_type_and_version)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_ERROR, parser.status);

	const uint8_t *msg_empty = NULL;

	/*
	 * If we provide a zero-length buffer, the parser should not do
	 * anything and return that it has consumed zero bytes.
	 * The state should not be altered in this case, as no data
	 * was consumed.
	 */
	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(0, parser.parse(&parser, msg_empty, 0));
	TEST_ASSERT_EQUAL(PARSER_STATUS_GOOD, parser.status);
}

TEST(aap_parser, parse_chunked_message)
{
	const uint8_t msg[] = {
		0x13, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
		'P', 'A', 'Y', 'L', 'O', 'A', 'D',
		'T', 'R', 'A', 'I', 'L', 'I', 'N', 'G', 'D', 'A', 'T', 'A',
	};
	__attribute__((unused))
	const uint8_t trailing_data[] = {
		'T', 'R', 'A', 'I', 'L', 'I', 'N', 'G', 'D', 'A', 'T', 'A',
	};
	const size_t msg_size = ARRAY_SIZE(msg) - ARRAY_SIZE(trailing_data);
	/*
	 * This splits the above message in several differently-sized chunks
	 * and checks whether the correct amount of bytes are read.
	 * The parser has to support partial reading of binary data such as the
	 * EID and payload, but can only support reading the number fields
	 * at once.
	 */
	const size_t chunk_sizes[] = {0, 2, 4, 4, 0, 7, 10, 100};
	const size_t deltas[] = {0, 1, 4, 3, 0, 0, 10, 5};
	size_t consumed, delta, c;

	aap_parser_reset(&parser);
	for (c = 0, consumed = 0; c < ARRAY_SIZE(chunk_sizes) &&
				  consumed < msg_size; c++) {
		TEST_ASSERT_EQUAL(PARSER_STATUS_GOOD, parser.status);
		delta = parse(
			msg + consumed,
			MIN(chunk_sizes[c], msg_size - consumed)
		);
		TEST_ASSERT_EQUAL(deltas[c], delta);
		consumed += delta;
	}
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(msg_size, consumed);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(chunk_sizes), c);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_SENDBUNDLE, parser.message.type);
	TEST_ASSERT_EQUAL(5, parser.message.eid_length);
	TEST_ASSERT(parser.message.eid[5] == '\0');
	TEST_ASSERT_EQUAL_STRING("UD3TN", parser.message.eid);
	TEST_ASSERT_EQUAL(7, parser.message.payload_length);
	TEST_ASSERT_EQUAL_MEMORY("PAYLOAD", parser.message.payload,
				 parser.message.payload_length);
	TEST_ASSERT(aap_message_is_valid(&parser.message));
}

TEST(aap_parser, parse_empty_payload_message)
{
	const uint8_t msg_empty_pl[] = {
		0x14, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_empty_pl),
			  parse(msg_empty_pl, ARRAY_SIZE(msg_empty_pl)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_RECVBUNDLE, parser.message.type);
	TEST_ASSERT_EQUAL(5, parser.message.eid_length);
	TEST_ASSERT(parser.message.eid[5] == '\0');
	TEST_ASSERT_EQUAL_STRING("UD3TN", parser.message.eid);
	TEST_ASSERT_EQUAL(0, parser.message.payload_length);
	TEST_ASSERT(aap_message_is_valid(&parser.message));
}

TEST(aap_parser, parse_empty_eid_message)
{
	const uint8_t msg_empty_eid[] = {
		0x12, 0x00, 0x00,
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg_empty_eid),
			  parse(msg_empty_eid, ARRAY_SIZE(msg_empty_eid)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_REGISTER, parser.message.type);
	TEST_ASSERT_EQUAL(0, parser.message.eid_length);
	TEST_ASSERT(parser.message.eid[0] == '\0');
	TEST_ASSERT(aap_message_is_valid(&parser.message));
}

TEST(aap_parser, extract_message)
{
	const uint8_t msg[] = {
		0x13, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
		'P', 'A', 'Y', 'L', 'O', 'A', 'D',
	};

	aap_parser_reset(&parser);
	TEST_ASSERT_EQUAL(ARRAY_SIZE(msg),
			  parse(msg, ARRAY_SIZE(msg)));
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, parser.status);
	TEST_ASSERT(aap_message_is_valid(&parser.message));

	struct aap_message msg_extracted = aap_parser_extract_message(&parser);

	TEST_ASSERT_NULL(parser.message.eid);
	TEST_ASSERT_NULL(parser.message.payload);
	TEST_ASSERT_NOT_NULL(msg_extracted.eid);
	TEST_ASSERT_NOT_NULL(msg_extracted.payload);
	TEST_ASSERT_EQUAL(AAP_MESSAGE_SENDBUNDLE, msg_extracted.type);
	TEST_ASSERT_EQUAL(5, msg_extracted.eid_length);
	TEST_ASSERT(msg_extracted.eid[5] == '\0');
	TEST_ASSERT_EQUAL_STRING("UD3TN", msg_extracted.eid);
	TEST_ASSERT_EQUAL(7, msg_extracted.payload_length);
	TEST_ASSERT_EQUAL_MEMORY("PAYLOAD", msg_extracted.payload,
				 msg_extracted.payload_length);
	TEST_ASSERT(aap_message_is_valid(&msg_extracted));
	aap_message_clear(&msg_extracted);
}

TEST_GROUP_RUNNER(aap_parser)
{
	RUN_TEST_CASE(aap_parser, init_and_reset);
	RUN_TEST_CASE(aap_parser, parse_ack_message);
	RUN_TEST_CASE(aap_parser, parse_nack_message);
	RUN_TEST_CASE(aap_parser, parse_register_message);
	RUN_TEST_CASE(aap_parser, parse_sendbundle_message);
	RUN_TEST_CASE(aap_parser, parse_recvbundle_message);
	RUN_TEST_CASE(aap_parser, parse_sendconfirm_message);
	RUN_TEST_CASE(aap_parser, parse_cancelbundle_message);
	RUN_TEST_CASE(aap_parser, parse_welcome_message);
	RUN_TEST_CASE(aap_parser, parse_ping_message);
	RUN_TEST_CASE(aap_parser, parse_invalid_message);
	RUN_TEST_CASE(aap_parser, parse_chunked_message);
	RUN_TEST_CASE(aap_parser, parse_empty_payload_message);
	RUN_TEST_CASE(aap_parser, parse_empty_eid_message);
	RUN_TEST_CASE(aap_parser, extract_message);
}
