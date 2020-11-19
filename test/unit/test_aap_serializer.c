#include "aap/aap.h"
#include "aap/aap_serializer.h"

#include "ud3tn/common.h"

#include "unity_fixture.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define VALID_MSG_COUNT 9

#define MSG_MAX_LENGTH 19

static struct aap_message valid_messages[VALID_MSG_COUNT];

static const size_t valid_message_lengths[VALID_MSG_COUNT] = {
	1,
	1,
	1 + 2 + 5,
	1 + 2 + 5 + 8 + 3,
	1 + 2 + 5 + 8 + 3,
	1 + 8,
	1 + 8,
	1 + 2 + 5,
	1,
};

static const uint8_t valid_message_bytes[VALID_MSG_COUNT][MSG_MAX_LENGTH] = {
	{0x10},
	{0x11},
	{0x12, 0x00, 0x05, 'U', 'D', '3', 'T', 'N'},
	{0x13, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 'P', 'L', '\0'},
	{0x14, 0x00, 0x05, 'U', 'D', '3', 'T', 'N',
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 'P', 'L', '\0'},
	{0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80},
	{0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80},
	{0x17, 0x00, 0x05, 'U', 'D', '3', 'T', 'N'},
	{0x18},
};

TEST_GROUP(aap_serializer);

TEST_SETUP(aap_serializer)
{
	struct aap_message init_valid_msgs[] = {
		(struct aap_message){
			.type = AAP_MESSAGE_ACK
		},
		(struct aap_message){
			.type = AAP_MESSAGE_NACK
		},
		(struct aap_message){
			.type = AAP_MESSAGE_REGISTER,
			.eid_length = 5,
			.eid = "UD3TN",
		},
		(struct aap_message){
			.type = AAP_MESSAGE_SENDBUNDLE,
			.eid_length = 5,
			.eid = "UD3TN",
			.payload_length = 3,
			.payload = (uint8_t *)"PL",
		},
		(struct aap_message){
			.type = AAP_MESSAGE_RECVBUNDLE,
			.eid_length = 5,
			.eid = "UD3TN",
			.payload_length = 3,
			.payload = (uint8_t *)"PL",
		},
		(struct aap_message){
			.type = AAP_MESSAGE_SENDCONFIRM,
			.bundle_id = 384, // 0x0180
		},
		(struct aap_message){
			.type = AAP_MESSAGE_CANCELBUNDLE,
			.bundle_id = 384, // 0x0180
		},
		(struct aap_message){
			.type = AAP_MESSAGE_WELCOME,
			.eid_length = 5,
			.eid = "UD3TN",
		},
		(struct aap_message){
			.type = AAP_MESSAGE_PING
		},
	};

	for (size_t i = 0; i < VALID_MSG_COUNT; i++)
		valid_messages[i] = init_valid_msgs[i];
}

TEST_TEAR_DOWN(aap_serializer)
{
}

TEST(aap_serializer, get_serialized_size)
{
	for (size_t c = 0; c < ARRAY_SIZE(valid_messages); c++) {
		TEST_ASSERT_EQUAL(valid_message_lengths[c],
				  aap_get_serialized_size(&valid_messages[c]));
	}
}

TEST(aap_serializer, serialize_into)
{
	uint8_t buffer[MSG_MAX_LENGTH];

	for (size_t c = 0; c < ARRAY_SIZE(valid_messages); c++) {
		aap_serialize_into(buffer, &valid_messages[c]);
		TEST_ASSERT_EQUAL_MEMORY(
			valid_message_bytes[c],
			buffer,
			valid_message_lengths[c]
		);
	}
}

TEST_GROUP_RUNNER(aap_serializer)
{
	RUN_TEST_CASE(aap_serializer, get_serialized_size);
	RUN_TEST_CASE(aap_serializer, serialize_into);
}
