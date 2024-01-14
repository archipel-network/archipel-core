// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "aap/aap.h"

#include "testud3tn_unity.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_EID1 "dtn://ud3tn.dtn/"
#define DEFAULT_EID2 "dtn://ud3tn.dtn/agent1"
#define DEFAULT_EID3 "ipn:42.0"
#define DEFAULT_AGENT_ID "agent1"

TEST_GROUP(aap);

TEST_SETUP(aap)
{
}

TEST_TEAR_DOWN(aap)
{
}

TEST(aap, validate_message)
{
	struct aap_message msg_ack = {
		.type = AAP_MESSAGE_ACK
	};
	struct aap_message msg_nack = {
		.type = AAP_MESSAGE_NACK
	};
	struct aap_message msg_register = {
		.type = AAP_MESSAGE_REGISTER,
		.eid_length = strlen(DEFAULT_AGENT_ID),
		.eid = DEFAULT_AGENT_ID,
	};
	struct aap_message msg_sendbundle = {
		.type = AAP_MESSAGE_SENDBUNDLE,
		.eid_length = strlen(DEFAULT_EID1),
		.eid = DEFAULT_EID1,
		.payload_length = 3,
		.payload = (uint8_t *)"PL",
	};
	struct aap_message msg_recvbundle = {
		.type = AAP_MESSAGE_RECVBUNDLE,
		.eid_length = strlen(DEFAULT_EID2),
		.eid = DEFAULT_EID2,
		.payload_length = 0,
		.payload = NULL, // empty payload
	};
	struct aap_message msg_sendconfirm = {
		.type = AAP_MESSAGE_SENDCONFIRM,
		.bundle_id = 800,
	};
	struct aap_message msg_cancelbundle = {
		.type = AAP_MESSAGE_CANCELBUNDLE,
		.bundle_id = 800,
	};
	struct aap_message msg_welcome = {
		.type = AAP_MESSAGE_WELCOME,
		.eid_length = strlen(DEFAULT_EID3),
		.eid = DEFAULT_EID3,
	};
	struct aap_message msg_ping = {
		.type = AAP_MESSAGE_PING
	};

	TEST_ASSERT(aap_message_is_valid(&msg_ack));
	TEST_ASSERT(aap_message_is_valid(&msg_nack));
	TEST_ASSERT(aap_message_is_valid(&msg_register));
	TEST_ASSERT(aap_message_is_valid(&msg_sendbundle));
	TEST_ASSERT(aap_message_is_valid(&msg_recvbundle));
	TEST_ASSERT(aap_message_is_valid(&msg_sendconfirm));
	TEST_ASSERT(aap_message_is_valid(&msg_cancelbundle));
	TEST_ASSERT(aap_message_is_valid(&msg_welcome));
	TEST_ASSERT(aap_message_is_valid(&msg_ping));

	struct aap_message msg_invalid_type = {
		.type = (enum aap_message_type)0x9
	};
	struct aap_message msg_invalid_eid_length = {
		.type = AAP_MESSAGE_REGISTER,
		.eid_length = UINT16_MAX + 1,
		.eid = "",
	};
	struct aap_message msg_invalid_eid = {
		.type = AAP_MESSAGE_WELCOME,
		.eid_length = strlen(DEFAULT_EID1) + 1,
		.eid = DEFAULT_EID1 "\0", // EID longer than strlen
	};
	struct aap_message msg_eid_null = {
		.type = AAP_MESSAGE_REGISTER,
		.eid_length = strlen(DEFAULT_EID1),
		.eid = NULL,
	};
	struct aap_message msg_payload_null = {
		.type = AAP_MESSAGE_SENDBUNDLE,
		.eid_length = strlen(DEFAULT_EID1),
		.eid = DEFAULT_EID1,
		.payload_length = 5,
		.payload = NULL,
	};

	TEST_ASSERT(!aap_message_is_valid(&msg_invalid_type));
	TEST_ASSERT(!aap_message_is_valid(&msg_invalid_eid_length));
	TEST_ASSERT(!aap_message_is_valid(&msg_invalid_eid));
	TEST_ASSERT(!aap_message_is_valid(&msg_eid_null));
	TEST_ASSERT(!aap_message_is_valid(&msg_payload_null));
}


TEST(aap, clear_message)
{
	struct aap_message msg = {
		.type = AAP_MESSAGE_SENDBUNDLE,
		.eid_length = 4,
		.eid = malloc(5),
		.payload_length = 3,
		.payload = malloc(3),
	};

	aap_message_clear(&msg);
	TEST_ASSERT(!aap_message_is_valid(&msg));
	TEST_ASSERT_NULL(msg.eid);
	TEST_ASSERT_NULL(msg.payload);
}

TEST_GROUP_RUNNER(aap)
{
	RUN_TEST_CASE(aap, validate_message);
	RUN_TEST_CASE(aap, clear_message);
}
