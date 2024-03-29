// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "aap/aap.h"
#include "aap/aap_serializer.h"

#include "cla/bibe_proto.h"

#include "ud3tn/common.h"

#include "testud3tn_unity.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ENC_BUNDLE_LEN 90
#define VALID_BPDU_LEN 95

static const uint8_t encapsulated_bundle_bytes[ENC_BUNDLE_LEN] = {
	0x9F, 0x89, 0x07, 0x00, 0x01, 0x82, 0x01, 0x77, 0x2F, 0x2F,
	0x75, 0x70, 0x70, 0x65, 0x72, 0x32, 0x2E, 0x64, 0x74, 0x6E,
	0x2F, 0x62, 0x75, 0x6E, 0x64, 0x6C, 0x65, 0x73, 0x69, 0x6E,
	0x6B, 0x82, 0x01, 0x6C, 0x2F, 0x2F, 0x73, 0x65, 0x6E, 0x64,
	0x65, 0x72, 0x2E, 0x64, 0x74, 0x6E, 0x82, 0x01, 0x00, 0x81,
	0x1B, 0x00, 0x00, 0x00, 0xA1, 0x34, 0x8A, 0xA2, 0x0B, 0x00,
	0x1A, 0x05, 0x26, 0x5C, 0x00, 0x42, 0xEE, 0xEB, 0x86, 0x01,
	0x01, 0x00, 0x02, 0x4A, 0x48, 0x61, 0x6C, 0x6C, 0x6F, 0x20,
	0x57, 0x65, 0x6C, 0x74, 0x44, 0x39, 0x50, 0xA7, 0x07, 0xFF
};

static const uint8_t valid_bpdu[VALID_BPDU_LEN] = {
	0x83, 0x00, 0x00, 0x58, 0x5A, 0x9F, 0x89, 0x07, 0x00, 0x01,
	0x82, 0x01, 0x77, 0x2F, 0x2F, 0x75, 0x70, 0x70, 0x65, 0x72,
	0x32, 0x2E, 0x64, 0x74, 0x6E, 0x2F, 0x62, 0x75, 0x6E, 0x64,
	0x6C, 0x65, 0x73, 0x69, 0x6E, 0x6B, 0x82, 0x01, 0x6C, 0x2F,
	0x2F, 0x73, 0x65, 0x6E, 0x64, 0x65, 0x72, 0x2E, 0x64, 0x74,
	0x6E, 0x82, 0x01, 0x00, 0x81, 0x1B, 0x00, 0x00, 0x00, 0xA1,
	0x34, 0x8A, 0xA2, 0x0B, 0x00, 0x1A, 0x05, 0x26, 0x5C, 0x00,
	0x42, 0xEE, 0xEB, 0x86, 0x01, 0x01, 0x00, 0x02, 0x4A, 0x48,
	0x61, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x65, 0x6C, 0x74, 0x44,
	0x39, 0x50, 0xA7, 0x07, 0xFF
};

TEST_GROUP(bibe_parser);

TEST_SETUP(bibe_parser)
{
}

TEST_TEAR_DOWN(bibe_parser)
{
}

TEST(bibe_parser, parse_bpdu)
{
	struct bibe_protocol_data_unit bpdu;

	size_t err = bibe_parser_parse(
		valid_bpdu,
		VALID_BPDU_LEN,
		&bpdu
	);

	TEST_ASSERT_EQUAL_INT(0, err);
	TEST_ASSERT_EQUAL_UINT64(0, bpdu.transmission_id);
	TEST_ASSERT_EQUAL_UINT64(0, bpdu.retransmission_time);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(
		encapsulated_bundle_bytes,
		bpdu.encapsulated_bundle,
		ENC_BUNDLE_LEN
	);
}

TEST_GROUP_RUNNER(bibe_parser)
{
	RUN_TEST_CASE(bibe_parser, parse_bpdu);
}
