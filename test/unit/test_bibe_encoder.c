// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "aap/aap.h"
#include "aap/aap_serializer.h"

#include "cla/bibe_proto.h"

#include "ud3tn/common.h"

#include "testud3tn_unity.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define EXPECTED_HEADER_LENGTH 31

static const uint8_t valid_header_bytes[EXPECTED_HEADER_LENGTH] = {
	0x19, 0x00, 0x0F, 0x64, 0x74, 0x6E, 0x3A, 0x2F, 0x2F, 0x75, 0x64,
	0x33, 0x74, 0x6E, 0x2E, 0x64, 0x74, 0x6E, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x5F, 0x83, 0x00, 0x00, 0x58, 0x5A
};
TEST_GROUP(bibe_header_encoder);

TEST_SETUP(bibe_header_encoder)
{
}

TEST_TEAR_DOWN(bibe_header_encoder)
{
}

TEST(bibe_header_encoder, get_encoded_size)
{
	struct bibe_header hdr;

	hdr = bibe_encode_header("dtn://ud3tn.dtn", 90);

	TEST_ASSERT_EQUAL(EXPECTED_HEADER_LENGTH, hdr.hdr_len);

	free(hdr.data);
}

TEST(bibe_header_encoder, encode_header)
{
	struct bibe_header hdr;

	hdr = bibe_encode_header("dtn://ud3tn.dtn", 90);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(
		valid_header_bytes,
		hdr.data,
		EXPECTED_HEADER_LENGTH
	);

	free(hdr.data);
}

TEST_GROUP_RUNNER(bibe_header_encoder)
{
	RUN_TEST_CASE(bibe_header_encoder, get_encoded_size);
	RUN_TEST_CASE(bibe_header_encoder, encode_header);
}
