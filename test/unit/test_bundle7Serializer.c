/**
 * Unit Test for BPbis serializer
 *
 * Raw CBOR data is loaded from "test_bundle7Data.c".
 */

#include "bundle7/serializer.h"
#include "bundle7/eid.h"
#include "bundle7/hopcount.h"
#include "bundle7/bundle_age.h"

#include "platform/hal_io.h"

#include "ud3tn/bundle.h"

#include "unity_fixture.h"

#include <stddef.h>
#include <stdlib.h>  // malloc(), free()
#include <string.h>  // memcpy()


TEST_GROUP(bundle7Serializer);

extern uint8_t cbor_simple_bundle[];
extern size_t len_simple_bundle;

static size_t output_bytes;


TEST_SETUP(bundle7Serializer)
{
	output_bytes = 0;
}

TEST_TEAR_DOWN(bundle7Serializer)
{
}


static void write(void *cla_obj, const void *data, const size_t len)
{
	char buf[32];

	TEST_ASSERT_TRUE_MESSAGE(
		output_bytes + len <= len_simple_bundle,
		"CBOR too long");

	snprintf(buf, sizeof(buf), "Offset was: %zu", output_bytes);

	if (len)
		TEST_ASSERT_EQUAL_INT8_ARRAY_MESSAGE(
			cbor_simple_bundle + output_bytes,
			data, len, buf);

	output_bytes += len;
}


TEST(bundle7Serializer, simple_bundle)
{
	struct bundle *bundle = bundle_init();

	TEST_ASSERT_NOT_NULL(bundle);

	bundle->protocol_version = 7;
	bundle->proc_flags = BUNDLE_FLAG_MUST_NOT_BE_FRAGMENTED
		| BUNDLE_FLAG_REPORT_DELIVERY
		// this is a RFC 5050 flag and should be ignored
		// by the serializer
		| BUNDLE_V6_FLAG_NORMAL_PRIORITY;
	bundle->crc_type = BUNDLE_CRC_TYPE_NONE;

	bundle->destination = strdup("dtn:GS2");
	bundle->source = strdup("ipn:243.350");
	bundle->report_to = strdup("dtn:none");

	bundle->creation_timestamp_ms = 658489863000; // 2020-11-12T09:51:03
	bundle->sequence_number = 0;
	bundle->lifetime_ms = 86400;

	struct bundle_block_list *prev;
	struct bundle_block_list *entry;
	struct bundle_block *block;

	// Previous node block
	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PREVIOUS_NODE);
	entry = bundle_block_entry_create(block);

	bundle->blocks = entry;
	prev = entry;

	// CBOR: [1, "GS4"]
	uint8_t previous_node[6] = {
		0x82, 0x01, 0x63, 0x47, 0x53, 0x34
	};

	block->number = 2;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = sizeof(previous_node);
	block->data = malloc(sizeof(previous_node));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, previous_node, sizeof(previous_node));

	// Hop count block
	block = bundle_block_create(BUNDLE_BLOCK_TYPE_HOP_COUNT);
	entry = bundle_block_entry_create(block);

	prev->next = entry;
	prev = entry;

	// CBOR: [30, 0]
	uint8_t hop_count[4] = { 0x82, 0x18, 0x1e, 0x00 };

	block->number = 3;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = sizeof(hop_count);
	block->data = malloc(sizeof(hop_count));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, hop_count, sizeof(hop_count));

	// Bundle age block
	block = bundle_block_create(BUNDLE_BLOCK_TYPE_BUNDLE_AGE);
	entry = bundle_block_entry_create(block);

	prev->next = entry;
	prev = entry;

	// CBOR: 0
	uint8_t bundle_age[1] = { 0x00 };

	block->number = 4;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = sizeof(bundle_age);
	block->data = malloc(sizeof(bundle_age));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, bundle_age, sizeof(bundle_age));

	// Payload
	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	entry = bundle_block_entry_create(block);

	prev->next = entry;

	uint8_t payload[12] = {
		'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!',
	};

	TEST_ASSERT_EQUAL(1, block->number);
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));
	bundle->payload_block = block;

	TEST_ASSERT_EQUAL(UD3TN_OK, bundle7_serialize(bundle, write, NULL));
	TEST_ASSERT_EQUAL(len_simple_bundle, output_bytes);

	bundle_free(bundle);
}

// -----------------------
// CRC 16 AX.25 Generation
// -----------------------
//
extern uint8_t cbor_crc16_primary_block[];
extern uint8_t cbor_crc16_payload_block[];
extern size_t len_crc16_primary_block;
extern size_t len_crc16_payload_block;

static void write_crc16_primary_block(void *cla_obj, const void *data,
	const size_t len)
{
	char buf[32];

	TEST_ASSERT_TRUE_MESSAGE(
		output_bytes + len <= len_crc16_primary_block,
		"CBOR too long");

	snprintf(buf, sizeof(buf), "Offset was: %zu", output_bytes);

	if (len)
		TEST_ASSERT_EQUAL_INT8_ARRAY_MESSAGE(
			cbor_crc16_primary_block + output_bytes,
			data, len, buf);

	output_bytes += len;
}

static void write_crc16_payload_block(void *cla_obj, const void *data,
	const size_t len)
{
	char buf[32];

	TEST_ASSERT_TRUE_MESSAGE(
		output_bytes + len <= len_crc16_payload_block,
		"CBOR too long");

	snprintf(buf, sizeof(buf), "Offset was: %zu", output_bytes);

	if (len)
		TEST_ASSERT_EQUAL_INT8_ARRAY_MESSAGE(
			cbor_crc16_payload_block + output_bytes,
			data, len, buf);

	output_bytes += len;
}

TEST(bundle7Serializer, crc16_generation)
{
	// --------------------
	// CRC-16 primary block
	// --------------------

	struct bundle *bundle = bundle_init();

	TEST_ASSERT_NOT_NULL(bundle);

	bundle->protocol_version = 7;
	bundle->proc_flags = BUNDLE_FLAG_NONE;
	bundle->crc_type = BUNDLE_CRC_TYPE_16;

	bundle->destination = strdup("dtn:GS2");
	bundle->source = strdup("dtn:none");
	bundle->report_to = strdup("dtn:none");

	bundle->creation_timestamp_ms = 0;
	bundle->sequence_number = 0;
	bundle->lifetime_ms = 86400;

	struct bundle_block *block;

	// Empty Payload
	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	bundle->blocks = bundle_block_entry_create(block);

	block->number = 0;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = 0;
	block->data = NULL;
	bundle->payload_block = block;

	TEST_ASSERT_EQUAL(UD3TN_OK,
		bundle7_serialize(bundle, write_crc16_primary_block, NULL));
	TEST_ASSERT_EQUAL(len_crc16_primary_block, output_bytes);

	// --------------------
	// CRC-16 payload block
	// --------------------

	// Replace payload
	free(block->data);

	const uint8_t payload[] = {
		'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!',
	};

	block->crc_type = BUNDLE_CRC_TYPE_16;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));

	// Disable CRC for primary block
	bundle->crc_type = BUNDLE_CRC_TYPE_NONE;

	// Reset output counter
	output_bytes = 0;

	TEST_ASSERT_EQUAL(UD3TN_OK,
		bundle7_serialize(bundle, write_crc16_payload_block, NULL));
	TEST_ASSERT_EQUAL(len_crc16_payload_block, output_bytes);

	bundle_free(bundle);
}

// --------------------------
// CRC 32 Ethernet Generation
// --------------------------
//
extern uint8_t cbor_crc32_primary_block[];
extern uint8_t cbor_crc32_payload_block[];
extern size_t len_crc32_primary_block;
extern size_t len_crc32_payload_block;

static void write_crc32_primary_block(void *cla_obj, const void *data,
	const size_t len)
{
	char buf[32];

	TEST_ASSERT_TRUE_MESSAGE(
		output_bytes + len <= len_crc32_primary_block,
		"CBOR too long");

	snprintf(buf, sizeof(buf), "Offset was: %zu", output_bytes);

	if (len)
		TEST_ASSERT_EQUAL_INT8_ARRAY_MESSAGE(
			cbor_crc32_primary_block + output_bytes,
			data, len, buf);

	output_bytes += len;
}

static void write_crc32_payload_block(void *cla_obj, const void *data,
	const size_t len)
{
	char buf[32];

	TEST_ASSERT_TRUE_MESSAGE(
		output_bytes + len <= len_crc32_payload_block,
		"CBOR too long");

	snprintf(buf, sizeof(buf), "Offset was: %zu", output_bytes);

	if (len)
		TEST_ASSERT_EQUAL_INT8_ARRAY_MESSAGE(
			cbor_crc32_payload_block + output_bytes,
			data, len, buf);

	output_bytes += len;
}

TEST(bundle7Serializer, crc32_generation)
{
	// --------------------
	// CRC-32 primary block
	// --------------------

	struct bundle *bundle = bundle_init();

	TEST_ASSERT_NOT_NULL(bundle);

	bundle->protocol_version = 7;
	bundle->proc_flags = BUNDLE_FLAG_NONE;
	bundle->crc_type = BUNDLE_CRC_TYPE_32;

	bundle->destination = strdup("dtn:GS2");
	bundle->source = strdup("dtn:none");
	bundle->report_to = strdup("dtn:none");

	bundle->creation_timestamp_ms = 0;
	bundle->sequence_number = 0;
	bundle->lifetime_ms = 86400;

	struct bundle_block *block;

	// Empty Payload
	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	bundle->blocks = bundle_block_entry_create(block);

	block->number = 0;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = 0;
	block->data = NULL;
	bundle->payload_block = block;

	TEST_ASSERT_EQUAL(UD3TN_OK,
		bundle7_serialize(bundle, write_crc32_primary_block, NULL));
	TEST_ASSERT_EQUAL(len_crc32_primary_block, output_bytes);

	// --------------------
	// CRC-32 payload block
	// --------------------

	// Replace payload
	free(block->data);

	const uint8_t payload[] = {
		'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!',
	};

	block->crc_type = BUNDLE_CRC_TYPE_32;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));

	// Disable CRC for primary block
	bundle->crc_type = BUNDLE_CRC_TYPE_NONE;

	// Reset output counter
	output_bytes = 0;

	TEST_ASSERT_EQUAL(UD3TN_OK,
		bundle7_serialize(bundle, write_crc32_payload_block, NULL));
	TEST_ASSERT_EQUAL(len_crc32_payload_block, output_bytes);

	bundle_free(bundle);
}


static uint8_t cbor_dtn_text[6] = { 0x82, 0x01, 0x63, 0x47, 0x53, 0x31 };

TEST(bundle7Serializer, dtn_text)
{
	size_t length;
	uint8_t *buffer;

	buffer = bundle7_eid_serialize_alloc("dtn:GS1", &length);
	TEST_ASSERT_NOT_NULL(buffer);
	TEST_ASSERT_EQUAL(sizeof(cbor_dtn_text), length);

	TEST_ASSERT_EQUAL_INT8_ARRAY(cbor_dtn_text, buffer,
		sizeof(cbor_dtn_text));
}


static const uint8_t dtn_none[] = { 0x82, 0x01, 0x00 };

TEST(bundle7Serializer, dtn_none)
{
	size_t length;
	uint8_t *buffer;

	buffer = bundle7_eid_serialize_alloc("dtn:none", &length);
	TEST_ASSERT_NOT_NULL(buffer);
	TEST_ASSERT_EQUAL(sizeof(dtn_none), length);

	TEST_ASSERT_EQUAL_INT8_ARRAY(dtn_none, buffer,
		sizeof(dtn_none));
}


static const uint8_t dtn_ipn[] = { 0x82, 0x02, 0x82, 0x0c, 0x18, 0x7b };

TEST(bundle7Serializer, dtn_ipn)
{
	size_t length;
	uint8_t *buffer;

	buffer = bundle7_eid_serialize_alloc("ipn:12.123", &length);
	TEST_ASSERT_NOT_NULL(buffer);
	TEST_ASSERT_EQUAL(sizeof(dtn_ipn), length);

	TEST_ASSERT_EQUAL_INT8_ARRAY(dtn_ipn, buffer,
		sizeof(dtn_ipn));
}


// [40, 10]
static const uint8_t cbor_hop_count[] = { 0x82, 0x18, 0x28, 0x0a };

TEST(bundle7Serializer, hop_count)
{
	struct bundle_hop_count hop_count = {
		.limit = 40,
		.count = 10
	};
	uint8_t *buffer = malloc(BUNDLE7_HOP_COUNT_MAX_ENCODED_SIZE);

	TEST_ASSERT_NOT_NULL(buffer)

	size_t written = bundle7_hop_count_serialize(
		&hop_count, buffer, BUNDLE7_HOP_COUNT_MAX_ENCODED_SIZE);

	TEST_ASSERT_EQUAL(sizeof(cbor_hop_count), written);
	TEST_ASSERT_EQUAL_INT8_ARRAY(cbor_hop_count, buffer,
		sizeof(cbor_hop_count));
}

// 42000
static const uint8_t cbor_bundle_age[] = { 0x19, 0xa4, 0x10 };

TEST(bundle7Serializer, bundle_age)
{
	uint64_t bundle_age = 42000;
	uint8_t *buffer = malloc(BUNDLE_AGE_MAX_ENCODED_SIZE);

	TEST_ASSERT_NOT_NULL(buffer);
	size_t written = bundle_age_serialize(bundle_age, buffer,
		BUNDLE_AGE_MAX_ENCODED_SIZE);

	TEST_ASSERT_EQUAL(sizeof(cbor_bundle_age), written);
	TEST_ASSERT_EQUAL_INT8_ARRAY(cbor_bundle_age, buffer,
		sizeof(cbor_bundle_age));
}


TEST_GROUP_RUNNER(bundle7Serializer)
{
	RUN_TEST_CASE(bundle7Serializer, dtn_text);
	RUN_TEST_CASE(bundle7Serializer, dtn_none);
	RUN_TEST_CASE(bundle7Serializer, dtn_ipn);
	RUN_TEST_CASE(bundle7Serializer, hop_count);
	RUN_TEST_CASE(bundle7Serializer, bundle_age);
	RUN_TEST_CASE(bundle7Serializer, simple_bundle);
	RUN_TEST_CASE(bundle7Serializer, crc16_generation);
	RUN_TEST_CASE(bundle7Serializer, crc32_generation);
}
