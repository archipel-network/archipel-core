#include "bundle7/create.h"

#include "ud3tn/bundle.h"

#include "platform/hal_time.h"

#include "unity_fixture.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static const char test_payload[] = "PAYLOAD";

TEST_GROUP(bundle7Create);

TEST_SETUP(bundle7Create)
{
}

TEST_TEAR_DOWN(bundle7Create)
{
}

TEST(bundle7Create, create_bundle)
{
	char *payload = malloc(sizeof(test_payload));
	uint64_t creation_timestamp_s = 658489863; // 2020-11-12T09:51:03+00:00

	memcpy(payload, test_payload, sizeof(test_payload));

	struct bundle *b = bundle7_create_local(
		payload, sizeof(test_payload),
		"dtn:sourceeid", "dtn:desteid",
		creation_timestamp_s, 1, 42,
		BUNDLE_FLAG_REPORT_DELIVERY);

	TEST_ASSERT_NOT_NULL(b);
	TEST_ASSERT_NOT_NULL(b->blocks);
	TEST_ASSERT_NOT_NULL(b->blocks->data);
	TEST_ASSERT_NULL(b->blocks->next);
	TEST_ASSERT_NOT_NULL(b->payload_block);
	TEST_ASSERT_EQUAL_PTR(b->payload_block, b->blocks->data);

	TEST_ASSERT_EQUAL(sizeof(test_payload),
			  b->payload_block->length);
	TEST_ASSERT_EQUAL_MEMORY(test_payload, b->payload_block->data,
				 sizeof(test_payload));
	TEST_ASSERT_EQUAL(1, b->payload_block->number);
	TEST_ASSERT_EQUAL(0,  b->payload_block->flags);

	TEST_ASSERT_EQUAL(7, b->protocol_version);
	TEST_ASSERT_EQUAL(BUNDLE_FLAG_REPORT_DELIVERY, b->proc_flags);
	TEST_ASSERT_EQUAL(creation_timestamp_s * 1000, b->creation_timestamp_ms);
	TEST_ASSERT_EQUAL(1, b->sequence_number);
	TEST_ASSERT_EQUAL(42000, b->lifetime_ms);
	TEST_ASSERT_NOT_EQUAL(0, b->primary_block_length);

	TEST_ASSERT_EQUAL_STRING("dtn:sourceeid", b->source);
	TEST_ASSERT_EQUAL_STRING("dtn:desteid", b->destination);
	TEST_ASSERT_EQUAL_STRING("dtn:none", b->report_to);

	bundle_free(b);
}

TEST_GROUP_RUNNER(bundle7Create)
{
	RUN_TEST_CASE(bundle7Create, create_bundle);
}
