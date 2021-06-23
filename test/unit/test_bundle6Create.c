#include "bundle6/create.h"

#include "ud3tn/bundle.h"

#include "platform/hal_time.h"

#include "unity_fixture.h"

#include <string.h>
#include <stdlib.h>

static const char test_payload[] = "PAYLOAD";

TEST_GROUP(bundle6Create);

TEST_SETUP(bundle6Create)
{
}

TEST_TEAR_DOWN(bundle6Create)
{
}

TEST(bundle6Create, create_bundle)
{
	char *payload = malloc(sizeof(test_payload));
	uint64_t creation_timestamp_s = 658489863; // 2020-11-12T09:51:03+00:00

	memcpy(payload, test_payload, sizeof(test_payload));

	struct bundle *b = bundle6_create_local(
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

	TEST_ASSERT_EQUAL_MEMORY(test_payload, b->payload_block->data,
				 sizeof(test_payload));
	TEST_ASSERT_EQUAL(BUNDLE_V6_BLOCK_FLAG_LAST_BLOCK,
			  b->payload_block->flags);

	TEST_ASSERT_EQUAL(6, b->protocol_version);
	TEST_ASSERT_EQUAL(BUNDLE_V6_FLAG_SINGLETON_ENDPOINT |
			  BUNDLE_FLAG_REPORT_DELIVERY, b->proc_flags);
	TEST_ASSERT_EQUAL(1, b->sequence_number);
	TEST_ASSERT_EQUAL(creation_timestamp_s * 1000, b->creation_timestamp_ms);
	TEST_ASSERT_EQUAL(42000, b->lifetime_ms);
	TEST_ASSERT_NOT_EQUAL(0, b->primary_block_length);

	TEST_ASSERT_NOT_NULL(b->source);
	TEST_ASSERT_EQUAL_STRING("dtn:sourceeid", b->source);
	TEST_ASSERT_NOT_NULL(b->destination);
	TEST_ASSERT_EQUAL_STRING("dtn:desteid", b->destination);
	TEST_ASSERT_NOT_NULL(b->report_to);
	TEST_ASSERT_EQUAL_STRING("dtn:none", b->report_to);
	TEST_ASSERT_NOT_NULL(b->current_custodian);
	TEST_ASSERT_EQUAL_STRING("dtn:none", b->current_custodian);

	bundle_free(b);
}

TEST(bundle6Create, fail_bundle_creation)
{
	// NOTE that bundle6_create_local takes over freeing the payload!
	TEST_ASSERT_NULL(bundle6_create_local(
		malloc(1), 1,
		"dtnsource", "dtn:dest",
		hal_time_get_timestamp_s(), 1, 42, 0
	));
	TEST_ASSERT_NULL(bundle6_create_local(
		malloc(1), 1,
		"dtn:source", "dtndest",
		hal_time_get_timestamp_s(), 1, 42, 0
	));
}

TEST_GROUP_RUNNER(bundle6Create)
{
	RUN_TEST_CASE(bundle6Create, create_bundle);
	RUN_TEST_CASE(bundle6Create, fail_bundle_creation);
}
