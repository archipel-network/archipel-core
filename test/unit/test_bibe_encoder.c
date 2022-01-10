#include "aap/aap.h"
#include "aap/aap_serializer.h"

#include "cla/bibe_proto.h"

#include "ud3tn/common.h"

#include "unity_fixture.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define EXPECTED_HEADER_LENGTH 33

static const uint8_t valid_header_bytes[EXPECTED_HEADER_LENGTH] = {
	0x19, 0x00, 0x0F, 0x64, 0x74, 0x6E, 0x3A, 0x2F, 0x2F, 0x75, 0x64,
	0x33, 0x74, 0x6E, 0x2E, 0x64, 0x74, 0x6E, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x61, 0x82, 0x03, 0x83, 0x00, 0x00, 0x58, 0x5A
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
	TEST_ASSERT_EQUAL(EXPECTED_HEADER_LENGTH,
				  bibe_encode_header("dtn://ud3tn.dtn", 90).hdr_len);
}

TEST(bibe_header_encoder, encode_header)
{
	struct bibe_header hdr;

	hdr = bibe_encode_header("dtn://ud3tn.dtn", 90);

	TEST_ASSERT_EQUAL_MEMORY(
		valid_header_bytes,
		hdr.data,
		EXPECTED_HEADER_LENGTH
	);
}

TEST_GROUP_RUNNER(bibe_header_encoder)
{
	RUN_TEST_CASE(bibe_header_encoder, get_encoded_size);
	RUN_TEST_CASE(bibe_header_encoder, encode_header);
}
