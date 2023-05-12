// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "bundle6/create.h"
#include "bundle6/serializer.h"
#include "bundle6/parser.h"

#include "ud3tn/bundle.h"
#include "ud3tn/node.h"

#include "platform/hal_time.h"

#include "unity_fixture.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static const char test_payload[] = "PAYLOAD";
static struct bundle *b;
static bool verify_ok;

TEST_GROUP(bundle6ParserSerializer);

TEST_SETUP(bundle6ParserSerializer)
{
	char *payload = malloc(sizeof(test_payload));
	uint64_t creation_timestamp_s = 658489863; // 2020-11-12T09:51:03+00:00

	memcpy(payload, test_payload, sizeof(test_payload));
	b = bundle6_create_local(
		payload,
		sizeof(test_payload),
		"dtn:sourceeid",
		"dtn:desteid",
		creation_timestamp_s * 1000, 1, 42 * 1000,
		BUNDLE_FLAG_REPORT_DELIVERY |
		BUNDLE_V6_FLAG_CUSTODY_TRANSFER_REQUESTED
	);
	b->report_to = strdup("dtn:reportto");
	b->current_custodian = strdup("dtn:custodian");

	struct bundle_block *block = bundle_block_create(
		BUNDLE_BLOCK_TYPE_HOP_COUNT
	);

	block->length = 1;
	block->data = malloc(1);
	block->flags = (
		BUNDLE_V6_BLOCK_FLAG_HAS_EID_REF_FIELD |
		BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED |
		BUNDLE_V6_BLOCK_FLAG_LAST_BLOCK
	);

	struct endpoint_list *eid_list = malloc(sizeof(struct endpoint_list));

	eid_list->eid = strdup("dtn:eidref1");
	eid_list->next = NULL;
	block->eid_refs = eid_list;
	b->blocks->next = bundle_block_entry_create(block);
	b->payload_block->flags = BUNDLE_BLOCK_FLAG_NONE;
}

TEST_TEAR_DOWN(bundle6ParserSerializer)
{
	bundle_free(b);
}

static void verify_bundle(struct bundle *b)
{
	TEST_ASSERT_NOT_NULL(b);

	// Check header
	TEST_ASSERT_EQUAL(6, b->protocol_version);
	TEST_ASSERT_EQUAL(
		BUNDLE_V6_FLAG_SINGLETON_ENDPOINT |
		BUNDLE_FLAG_REPORT_DELIVERY |
		BUNDLE_V6_FLAG_CUSTODY_TRANSFER_REQUESTED,
		b->proc_flags
	);
	TEST_ASSERT_EQUAL(1, b->sequence_number);
	TEST_ASSERT_EQUAL(658489863000, b->creation_timestamp_ms);
	TEST_ASSERT_EQUAL(42000, b->lifetime_ms);
	TEST_ASSERT_NOT_EQUAL(0, b->primary_block_length);

	TEST_ASSERT_NOT_NULL(b->source);
	TEST_ASSERT_EQUAL_STRING("dtn:sourceeid", b->source);
	TEST_ASSERT_NOT_NULL(b->destination);
	TEST_ASSERT_EQUAL_STRING("dtn:desteid", b->destination);
	TEST_ASSERT_NOT_NULL(b->report_to);
	TEST_ASSERT_EQUAL_STRING("dtn:reportto", b->report_to);
	TEST_ASSERT_NOT_NULL(b->current_custodian);
	TEST_ASSERT_EQUAL_STRING("dtn:custodian", b->current_custodian);

	// Check blocks
	TEST_ASSERT_NOT_NULL(b->blocks);
	TEST_ASSERT_NOT_NULL(b->blocks->data);
	TEST_ASSERT_NOT_NULL(b->blocks->next);
	TEST_ASSERT_NOT_NULL(b->blocks->next->data);
	TEST_ASSERT_NULL(b->blocks->next->next);
	TEST_ASSERT_NOT_NULL(b->payload_block);
	TEST_ASSERT_EQUAL_PTR(b->payload_block, b->blocks->data);

	// Check payload block
	TEST_ASSERT_EQUAL(
		BUNDLE_BLOCK_FLAG_NONE,
		b->payload_block->flags
	);
	TEST_ASSERT_EQUAL(
		sizeof(test_payload),
		b->payload_block->length
	);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(
		test_payload,
		b->payload_block->data,
		sizeof(test_payload)
	);

	// Check next block with EID refs
	TEST_ASSERT_EQUAL(
		BUNDLE_V6_BLOCK_FLAG_HAS_EID_REF_FIELD |
		BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED |
		BUNDLE_V6_BLOCK_FLAG_LAST_BLOCK,
		b->blocks->next->data->flags
	);
	TEST_ASSERT_EQUAL(1, b->blocks->next->data->length);
	TEST_ASSERT_NOT_NULL(b->blocks->next->data->eid_refs);
	TEST_ASSERT_NOT_NULL(b->blocks->next->data->eid_refs->eid);
	TEST_ASSERT_NULL(b->blocks->next->data->eid_refs->next);
	TEST_ASSERT_EQUAL_STRING(
		"dtn:eidref1",
		b->blocks->next->data->eid_refs->eid
	);

	verify_ok = true;
}

static void verify_and_free_bundle(struct bundle *b, void *param)
{
	(void)param;
	verify_bundle(b);
	bundle_free(b);
}

struct buf_info {
	uint8_t *buf;
	size_t pos;
};

static void _write(void *buf_info, const void *data, const size_t len)
{
	struct buf_info *bi = buf_info;

	memcpy(&bi->buf[bi->pos], data, len);
	bi->pos += len;
}

TEST(bundle6ParserSerializer, parse_and_serialize)
{
	verify_bundle(b);
	TEST_ASSERT_TRUE(verify_ok);

	const size_t serialized_size_before = bundle_get_serialized_size(b);
	uint8_t *serializebuffer = malloc(serialized_size_before);
	struct buf_info bi = {
		.buf = serializebuffer,
		.pos = 0,
	};

	TEST_ASSERT_NOT_NULL(serializebuffer);
	TEST_ASSERT_NOT_EQUAL(0, serialized_size_before);
	bundle6_serialize(b, _write, &bi);

	const size_t serialized_size_after = bi.pos;

	TEST_ASSERT_EQUAL(serialized_size_before, serialized_size_after);

	struct bundle6_parser p;

	bundle6_parser_init(&p, verify_and_free_bundle, NULL);
	verify_ok = false;
	bundle6_parser_read(&p, serializebuffer, serialized_size_after);
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, p.basedata->status);
	TEST_ASSERT_TRUE(verify_ok);
	bundle6_parser_reset(&p);
	TEST_ASSERT_EQUAL(PARSER_STATUS_GOOD, p.basedata->status);

	free(serializebuffer);
}

// Bundle sourced and captured from ION v3.7.4, see #7 for details
static uint8_t ION_TEST_BUNDLE_IPN_CBHE[] = {
	// Primary Block
	// Version
	0x06,
	// Processing Flags
	0x81, 0x14,
	// Length
	0x11,
	// Destination: ipn:3.1 (CBHE)
	0x03, 0x01,
	// Source: dtn:none (CBHE)
	0x00, 0x00,
	// Report-to: dtn:none (CBHE)
	0x00, 0x00,
	// Custodian: dtn:none (CBHE)
	0x00, 0x00,
	// Timestamp
	0x82, 0xDC, 0xAE, 0xC7, 0x2A,
	// Seqnum
	0x01,
	// Lifetime
	0x82, 0x2C,
	// Dict. Length
	0x00,

	// 2nd Block (type 5)
	0x05,
	// Flags: discard if unproc
	0x10,
	// Length: 8 bytes
	0x08,
	// Data
	0x69, 0x70, 0x6E, 0x00, 0x31, 0x2E, 0x30, 0x00,

	// 3rd block (type 20)
	0x14,
	// Flags: replicate in every fragment
	0x01,
	// Length: 1 byte
	0x01,
	// Data
	0x00,

	// Payload Block (type 1)
	0x01,
	// Flags: "last block", "must be replicated"
	0x09,
	// Length: 10 bytes
	0x0A,
	// "hallo welt"
	0x68, 0x61, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x65, 0x6C, 0x74
};

static void verify_and_free_ipn_cbhe_bundle(struct bundle *b, void *param)
{
	(void)param;

	TEST_ASSERT_NOT_NULL(b);
	TEST_ASSERT_NOT_NULL(b->payload_block);
	TEST_ASSERT_NOT_NULL(b->blocks);
	TEST_ASSERT_NOT_NULL(b->blocks->data);
	TEST_ASSERT_NOT_NULL(b->blocks->next);
	TEST_ASSERT_NOT_NULL(b->blocks->next->data);
	TEST_ASSERT_NOT_NULL(b->blocks->next->next);
	TEST_ASSERT_NOT_NULL(b->blocks->next->next->data);
	TEST_ASSERT_NULL(b->blocks->next->next->next);

	TEST_ASSERT_EQUAL(6, b->protocol_version);
	TEST_ASSERT_EQUAL(
		BUNDLE_V6_FLAG_SINGLETON_ENDPOINT |
		BUNDLE_FLAG_MUST_NOT_BE_FRAGMENTED |
		BUNDLE_V6_FLAG_NORMAL_PRIORITY,
		b->proc_flags
	);
	TEST_ASSERT_EQUAL(1, b->sequence_number);
	TEST_ASSERT_EQUAL(300000, b->lifetime_ms);
	TEST_ASSERT_NOT_EQUAL(10, b->primary_block_length);

	TEST_ASSERT_EQUAL_STRING("ipn:3.1", b->destination);
	TEST_ASSERT_EQUAL_STRING("dtn:none", b->source);
	TEST_ASSERT_EQUAL_STRING("dtn:none", b->report_to);
	TEST_ASSERT_EQUAL_STRING("dtn:none", b->current_custodian);

	TEST_ASSERT_EQUAL_PTR(b->payload_block, b->blocks->next->next->data);

	TEST_ASSERT_EQUAL(BUNDLE_BLOCK_TYPE_PAYLOAD, b->payload_block->type);
	TEST_ASSERT_EQUAL(
		BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED |
		BUNDLE_V6_BLOCK_FLAG_LAST_BLOCK,
		b->payload_block->flags
	);
	TEST_ASSERT_EQUAL_UINT8_ARRAY("hallo welt", b->payload_block->data, 10);

	// other blocks
	TEST_ASSERT_EQUAL(
		BUNDLE_BLOCK_FLAG_DISCARD_IF_UNPROC,
		b->blocks->data->flags
	);

	bundle_free(b);
}

TEST(bundle6ParserSerializer, parse_cbhe)
{
	struct bundle6_parser p;

	bundle6_parser_init(&p, verify_and_free_ipn_cbhe_bundle, NULL);
	bundle6_parser_read(
		&p,
		ION_TEST_BUNDLE_IPN_CBHE,
		sizeof(ION_TEST_BUNDLE_IPN_CBHE)
	);
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, p.basedata->status);
	bundle6_parser_reset(&p);
	TEST_ASSERT_EQUAL(PARSER_STATUS_GOOD, p.basedata->status);
}

TEST_GROUP_RUNNER(bundle6ParserSerializer)
{
	RUN_TEST_CASE(bundle6ParserSerializer, parse_and_serialize);
	RUN_TEST_CASE(bundle6ParserSerializer, parse_cbhe);
}
