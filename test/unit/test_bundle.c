// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "bundle6/bundle6.h"
#include "bundle7/bundle7.h"

#include "ud3tn/bundle.h"
#include "ud3tn/config.h"

#include "testud3tn_unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TEST_GROUP(bundle);

TEST_SETUP(bundle)
{
}

TEST_TEAR_DOWN(bundle)
{
}

TEST(bundle, bundle_init)
{
	struct bundle *bundle = bundle_init();

	TEST_ASSERT_NOT_NULL(bundle);
	bundle_free(bundle);
}

TEST(bundle, bundle_reset)
{
	struct bundle *bundle = NULL;

	bundle_reset(bundle);

	bundle = malloc(sizeof(struct bundle));

	TEST_ASSERT_NOT_NULL(bundle);

	bundle->protocol_version = 7;
	bundle->proc_flags = BUNDLE_FLAG_ACKNOWLEDGEMENT_REQUESTED;
	bundle->ret_constraints = BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED;

	bundle->destination = strdup("dtn:GS2");
	bundle->source = strdup("ipnd:243.350");
	bundle->report_to = strdup("dtn:none");
	bundle->current_custodian = strdup("dtn:GS2");

	bundle->crc_type = BUNDLE_CRC_TYPE_32;
	bundle->creation_timestamp_ms = 5;
	bundle->reception_timestamp_ms = 10;
	bundle->sequence_number = 15;
	bundle->lifetime_ms = 86400;
	bundle->fragment_offset = 24;
	bundle->total_adu_length = 3;
	bundle->primary_block_length = 8;

	struct bundle_block_list *bundle_block_list = malloc(sizeof(struct bundle_block_list));

	bundle_block_list->data = NULL;
	bundle_block_list->next = NULL;

	bundle->blocks = bundle_block_list;

	struct bundle_block *payload_block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);

	bundle->payload_block = payload_block;

	TEST_ASSERT_NOT_NULL(bundle->blocks);
	TEST_ASSERT_NOT_NULL(bundle->payload_block);

	bundle_reset(bundle);

	TEST_ASSERT_EQUAL(bundle->protocol_version, 0x06);
	TEST_ASSERT_EQUAL(bundle->proc_flags, BUNDLE_FLAG_NONE);
	TEST_ASSERT_EQUAL(bundle->ret_constraints, BUNDLE_RET_CONSTRAINT_NONE);

	TEST_ASSERT_EQUAL(bundle->destination, NULL);
	TEST_ASSERT_EQUAL(bundle->source, NULL);
	TEST_ASSERT_EQUAL(bundle->report_to, NULL);
	TEST_ASSERT_EQUAL(bundle->current_custodian, NULL);

	TEST_ASSERT_EQUAL(bundle->crc_type, DEFAULT_CRC_TYPE);
	TEST_ASSERT_EQUAL(bundle->creation_timestamp_ms, 0);
	TEST_ASSERT_EQUAL(bundle->reception_timestamp_ms, 0);
	TEST_ASSERT_EQUAL(bundle->sequence_number, 0);
	TEST_ASSERT_EQUAL(bundle->lifetime_ms, 0);
	TEST_ASSERT_EQUAL(bundle->fragment_offset, 0);
	TEST_ASSERT_EQUAL(bundle->total_adu_length, 0);
	TEST_ASSERT_EQUAL(bundle->primary_block_length, 0);
	TEST_ASSERT_EQUAL(bundle->blocks, NULL);
	TEST_ASSERT_EQUAL(bundle->payload_block, NULL);

	bundle_free(bundle);
}

TEST(bundle, bundle_free)
{
	struct bundle *bundle = NULL;

	bundle_free(bundle);
}

TEST(bundle, bundle_drop)
{
	struct bundle *bundle2 = bundle_init();
	struct bundle *bundle3 = bundle_init();

	bundle2->ret_constraints = BUNDLE_RET_CONSTRAINT_NONE;

	bundle_drop(bundle2);
	bundle_drop(bundle3);
}

TEST(bundle, bundle_copy_headers)
{
	struct bundle *to = malloc(sizeof(struct bundle));
	struct bundle *from = bundle_init();

	from->destination = strdup("dtn:GS3");
	from->source = strdup("dtn:GS1");
	from->report_to = strdup("dtn:none");
	from->current_custodian = strdup("dtn:GS2");
	from->destination = strdup("dtn:GS4");

	bundle_copy_headers(to, from);

	TEST_ASSERT_EQUAL_STRING(to->destination, from->destination);
	TEST_ASSERT_EQUAL_STRING(to->source, from->source);
	TEST_ASSERT_EQUAL_STRING(to->report_to, from->report_to);
	TEST_ASSERT_EQUAL_STRING(to->current_custodian, from->current_custodian);

	bundle_free(to);
	bundle_free(from);
}

TEST(bundle, bundle_recalculate_header_length)
{
	struct bundle *bundle_fail = bundle_init();

	bundle_fail->protocol_version = 5;

	TEST_ASSERT_EQUAL(UD3TN_FAIL, bundle_recalculate_header_length(bundle_fail));

	struct bundle *bundle = malloc(sizeof(struct bundle));

	TEST_ASSERT_NOT_NULL(bundle);

	bundle->protocol_version = 6;
	bundle->proc_flags = BUNDLE_FLAG_ACKNOWLEDGEMENT_REQUESTED;
	bundle->ret_constraints = BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED;

	bundle->destination = strdup("dtn:GS2");
	bundle->source = strdup("ipnd:243.350");
	bundle->report_to = strdup("dtn:none");
	bundle->current_custodian = strdup("dtn:GS2");

	bundle->crc_type = BUNDLE_CRC_TYPE_32;
	bundle->creation_timestamp_ms = 5;
	bundle->reception_timestamp_ms = 10;
	bundle->sequence_number = 15;
	bundle->lifetime_ms = 86400;
	bundle->fragment_offset = 24;
	bundle->total_adu_length = 3;
	bundle->primary_block_length = 8;

	struct bundle_block_list *bundle_block_list = malloc(sizeof(struct bundle_block_list));

	bundle_block_list->data = bundle_block_create(0);
	bundle_block_list->next = NULL;

	bundle->blocks = bundle_block_list;

	struct bundle_block *payload_block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);

	bundle->payload_block = payload_block;

	TEST_ASSERT_NOT_NULL(bundle->blocks);
	TEST_ASSERT_NOT_NULL(bundle->payload_block);

	TEST_ASSERT_EQUAL(UD3TN_OK, bundle_recalculate_header_length(bundle));

	bundle_free(bundle_fail);
	bundle_free(bundle);
}

TEST(bundle, bundle_dup)
{
	struct bundle *bundle = NULL;

	TEST_ASSERT_NULL(bundle_dup(bundle));

	bundle = malloc(sizeof(struct bundle));

	TEST_ASSERT_NOT_NULL(bundle);

	bundle->protocol_version = 6;
	bundle->proc_flags = BUNDLE_FLAG_ACKNOWLEDGEMENT_REQUESTED;
	bundle->ret_constraints = BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED;

	bundle->destination = strdup("dtn:GS2");
	bundle->source = strdup("ipnd:243.350");
	bundle->report_to = strdup("dtn:none");
	bundle->current_custodian = strdup("dtn:GS2");

	bundle->crc_type = BUNDLE_CRC_TYPE_32;
	bundle->creation_timestamp_ms = 5;
	bundle->reception_timestamp_ms = 10;
	bundle->sequence_number = 15;
	bundle->lifetime_ms = 86400;
	bundle->fragment_offset = 24;
	bundle->total_adu_length = 3;
	bundle->primary_block_length = 8;

	struct bundle_block_list *prev;
	struct bundle_block_list *entry;
	struct bundle_block *block;

	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PREVIOUS_NODE);
	entry = bundle_block_entry_create(block);

	bundle->blocks = entry;
	prev = entry;

	uint8_t previous_node[6] = {
		0x82, 0x01, 0x63, 0x47, 0x53, 0x34
	};

	block->number = 2;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(previous_node);
	block->data = malloc(sizeof(previous_node));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, previous_node, sizeof(previous_node));

	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	entry = bundle_block_entry_create(block);

	prev->next = entry;

	uint8_t payload[13] = {
		0x4c, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72,
		0x6c, 0x64, 0x21
	};

	block->number = 0;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));
	bundle->payload_block = block;
	entry->next = NULL;

	TEST_ASSERT_NOT_NULL(bundle->blocks);
	TEST_ASSERT_NOT_NULL(bundle->payload_block);

	struct bundle *bundle_duplication = bundle_dup(bundle);

	TEST_ASSERT_EQUAL(bundle->protocol_version, bundle_duplication->protocol_version);
	TEST_ASSERT_EQUAL(bundle->proc_flags, bundle_duplication->proc_flags);
	TEST_ASSERT_EQUAL(bundle->ret_constraints, bundle_duplication->ret_constraints);

	TEST_ASSERT_EQUAL_STRING(bundle->destination, bundle_duplication->destination);
	TEST_ASSERT_EQUAL_STRING(bundle->source, bundle_duplication->source);
	TEST_ASSERT_EQUAL_STRING(bundle->report_to, bundle_duplication->report_to);
	TEST_ASSERT_EQUAL_STRING(bundle->current_custodian, bundle_duplication->current_custodian);

	TEST_ASSERT_EQUAL(bundle->crc_type, bundle_duplication->crc_type);
	TEST_ASSERT_EQUAL(bundle->creation_timestamp_ms, bundle_duplication->creation_timestamp_ms);
	TEST_ASSERT_EQUAL(bundle->reception_timestamp_ms, bundle_duplication->reception_timestamp_ms);
	TEST_ASSERT_EQUAL(bundle->sequence_number, bundle_duplication->sequence_number);
	TEST_ASSERT_EQUAL(bundle->lifetime_ms, bundle_duplication->lifetime_ms);
	TEST_ASSERT_EQUAL(bundle->fragment_offset, bundle_duplication->fragment_offset);
	TEST_ASSERT_EQUAL(bundle->total_adu_length, bundle_duplication->total_adu_length);
	TEST_ASSERT_EQUAL(bundle->primary_block_length, bundle_duplication->primary_block_length);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(bundle->blocks->data, bundle_duplication->blocks->data, sizeof(bundle->blocks->data));
	TEST_ASSERT_EQUAL_UINT8_ARRAY(bundle->payload_block->data, bundle_duplication->payload_block->data, sizeof(bundle->payload_block->data));

	bundle_free(bundle);
	bundle_free(bundle_duplication);
}

TEST(bundle, bundle_get_routing_priority)
{
	struct bundle *bundle = bundle_init();

	TEST_ASSERT_NOT_NULL(bundle);
	TEST_ASSERT_EQUAL(bundle_get_routing_priority(bundle), BUNDLE_RPRIO_LOW);

	bundle->ret_constraints = BUNDLE_RET_CONSTRAINT_FLAG_OWN;
	TEST_ASSERT_EQUAL(bundle_get_routing_priority(bundle), BUNDLE_RPRIO_HIGH);

	bundle->ret_constraints = BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED;
	TEST_ASSERT_EQUAL(bundle_get_routing_priority(bundle), BUNDLE_RPRIO_HIGH);

	bundle->ret_constraints = BUNDLE_RET_CONSTRAINT_NONE;
	bundle->proc_flags = BUNDLE_V6_FLAG_EXPEDITED_PRIORITY;
	TEST_ASSERT_EQUAL(bundle_get_routing_priority(bundle), BUNDLE_RPRIO_HIGH);

	bundle->proc_flags = BUNDLE_V6_FLAG_NORMAL_PRIORITY;
	TEST_ASSERT_EQUAL(bundle_get_routing_priority(bundle), BUNDLE_RPRIO_NORMAL);

	bundle->proc_flags = BUNDLE_FLAG_NONE;
	bundle->protocol_version = 7;
	TEST_ASSERT_EQUAL(bundle_get_routing_priority(bundle), BUNDLE_RPRIO_NORMAL);

	bundle_free(bundle);
}

TEST(bundle, bundle_get_serialized_size)
{
	struct bundle *bundle = bundle_init();

	TEST_ASSERT_NOT_NULL(bundle);
	bundle->protocol_version = 5;

	TEST_ASSERT_EQUAL(0, bundle_get_serialized_size(bundle));
	bundle_free(bundle);
}

TEST(bundle, bundle_list_entry_create)
{
	struct bundle *bundle = NULL;

	TEST_ASSERT_NULL(bundle_list_entry_create(bundle));

	bundle = bundle_init();

	struct bundle_list *bundle_list = bundle_list_entry_create(bundle);

	TEST_ASSERT_EQUAL(bundle_list->data, bundle);
	TEST_ASSERT_NULL(bundle_list->next);
	bundle_free(bundle);

}

TEST(bundle, bundle_list_entry_free)
{
	struct bundle_list *bundle_list_entry = NULL;
	struct bundle *bundle = bundle_init();

	TEST_ASSERT_NULL(bundle_list_entry_free(bundle_list_entry));

	bundle_list_entry = bundle_list_entry_create(bundle);

	TEST_ASSERT_NOT_NULL(bundle_list_entry->data);
	TEST_ASSERT_NULL(bundle_list_entry->next);

	TEST_ASSERT_NULL(bundle_list_entry_free(bundle_list_entry));
}

TEST(bundle, bundle_block_find_first_by_type)
{
	struct bundle_block_list *blocks = NULL;

	TEST_ASSERT_NULL(bundle_block_find_first_by_type(blocks, 0));

	struct bundle_block *payload1 = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	struct bundle_block *payload2 = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	struct bundle_block *hop_c = bundle_block_create(BUNDLE_BLOCK_TYPE_HOP_COUNT);
	struct bundle_block *test_block = NULL;
	uint8_t data = 5;

	payload1->data = &data;
	blocks = bundle_block_entry_create(payload1);
	blocks->next = bundle_block_entry_create(payload2);
	blocks->next->next = bundle_block_entry_create(hop_c);

	test_block = bundle_block_find_first_by_type(blocks, BUNDLE_BLOCK_TYPE_PAYLOAD);

	TEST_ASSERT_EQUAL(payload1, test_block);

	test_block = bundle_block_find_first_by_type(blocks, BUNDLE_BLOCK_TYPE_HOP_COUNT);

	TEST_ASSERT_EQUAL(hop_c, test_block);

	test_block = bundle_block_find_first_by_type(blocks, BUNDLE_BLOCK_TYPE_PREVIOUS_NODE);
	TEST_ASSERT_NULL(test_block);
}

TEST(bundle, bundle_block_entry_create)
{
	struct bundle_block *b = NULL;

	TEST_ASSERT_NULL(bundle_block_entry_create(b));

	b = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);

	struct bundle_block_list *bundle_block_list = bundle_block_entry_create(b);

	TEST_ASSERT_EQUAL(b, bundle_block_list->data);
	TEST_ASSERT_NULL(bundle_block_list->next);
}

TEST(bundle, bundle_block_entry_free)
{
	struct bundle_block *b = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	struct bundle_block_list *bundle_block_list = NULL;

	TEST_ASSERT_NULL(bundle_block_entry_free(bundle_block_list));

	bundle_block_list = bundle_block_entry_create(b);
	bundle_block_entry_free(bundle_block_list);

}

TEST(bundle, bundle_block_dup)
{
	struct bundle_block *block = NULL;
	struct bundle_block *bundle_block_duplication = NULL;

	TEST_ASSERT_NULL(bundle_block_dup(block));

	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);

	uint8_t payload[13] = {
		0x4c, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72,
		0x6c, 0x64, 0x21
	};

	block->number = 0;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));

	bundle_block_duplication = bundle_block_dup(block);

	TEST_ASSERT_EQUAL(block->type, bundle_block_duplication->type);
	TEST_ASSERT_EQUAL(block->number, bundle_block_duplication->number);
	TEST_ASSERT_EQUAL(block->flags, bundle_block_duplication->flags);
	TEST_ASSERT_EQUAL(block->length, bundle_block_duplication->length);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(block->data, bundle_block_duplication->data, sizeof(block->length));
	TEST_ASSERT_EQUAL(block->eid_refs, bundle_block_duplication->eid_refs);
	TEST_ASSERT_EQUAL(block->crc_type, bundle_block_duplication->crc_type);

}

TEST(bundle, bundle_block_entry_dup)
{
	struct bundle_block_list *e = NULL;
	struct bundle_block *b = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	struct bundle_block_list *copy = NULL;
	uint8_t data = 5;

	b->data = &data;
	TEST_ASSERT_NULL(bundle_block_entry_dup(e));

	e = bundle_block_entry_create(b);
	copy = bundle_block_entry_dup(e);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(e->data, copy->data, sizeof(data));
	TEST_ASSERT_EQUAL(e->next, copy->next);
}

TEST(bundle, bundle_block_list_dup)
{
	struct bundle_block_list *blocks = NULL;

	TEST_ASSERT_NULL(bundle_block_list_dup(blocks));

	blocks = malloc(sizeof(struct bundle_block_list));

	struct bundle_block_list *prev = NULL;
	struct bundle_block_list *entry = NULL;
	struct bundle_block *block = NULL;

	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PREVIOUS_NODE);
	entry = bundle_block_entry_create(block);
	blocks = entry;
	prev = entry;
	uint8_t previous_node[6] = {
		0x82, 0x01, 0x63, 0x47, 0x53, 0x34
	};

	block->number = 2;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = sizeof(previous_node);
	block->data = malloc(sizeof(previous_node));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, previous_node, sizeof(previous_node));

	block = bundle_block_create(BUNDLE_BLOCK_TYPE_HOP_COUNT);
	entry = bundle_block_entry_create(block);

	uint8_t hop_count[4] = { 0x82, 0x18, 0x1e, 0x00 };

	block->number = 3;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = sizeof(hop_count);
	block->data = malloc(sizeof(hop_count));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, hop_count, sizeof(hop_count));

	prev->next = entry;
	prev = entry;

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

	struct bundle_block_list *bundle_block_list_dpl = bundle_block_list_dup(blocks);

	TEST_ASSERT_NOT_NULL(bundle_block_list_dpl);
}

TEST(bundle, bundle_get_fragment_min_size)
{
	struct bundle *bundle = bundle_init();

	bundle->protocol_version = 5;

	TEST_ASSERT_EQUAL(0, bundle_get_first_fragment_min_size(bundle));
	TEST_ASSERT_EQUAL(0, bundle_get_mid_fragment_min_size(bundle));
	TEST_ASSERT_EQUAL(0, bundle_get_last_fragment_min_size(bundle));

	bundle->protocol_version = 6;
	bundle->proc_flags = BUNDLE_FLAG_ACKNOWLEDGEMENT_REQUESTED;
	bundle->ret_constraints = BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED;

	bundle->destination = strdup("dtn:GS2");
	bundle->source = strdup("ipnd:243.350");
	bundle->report_to = strdup("dtn:none");
	bundle->current_custodian = strdup("dtn:GS2");

	bundle->crc_type = BUNDLE_CRC_TYPE_32;
	bundle->creation_timestamp_ms = 5;
	bundle->reception_timestamp_ms = 10;
	bundle->sequence_number = 15;
	bundle->lifetime_ms = 86400;
	bundle->fragment_offset = 24;
	bundle->total_adu_length = 3;
	bundle->primary_block_length = 8;

	struct bundle_block_list *prev;
	struct bundle_block_list *entry;
	struct bundle_block *block;

	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PREVIOUS_NODE);
	entry = bundle_block_entry_create(block);

	bundle->blocks = entry;
	prev = entry;

	uint8_t previous_node[6] = {
		0x82, 0x01, 0x63, 0x47, 0x53, 0x34
	};

	block->number = 2;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(previous_node);
	block->data = malloc(sizeof(previous_node));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, previous_node, sizeof(previous_node));

	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	entry = bundle_block_entry_create(block);

	prev->next = entry;

	uint8_t payload[13] = {
		0x4c, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72,
		0x6c, 0x64, 0x21
	};

	block->number = 0;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));
	bundle->payload_block = block;

	TEST_ASSERT_EQUAL(bundle6_get_first_fragment_min_size(bundle), bundle_get_first_fragment_min_size(bundle));
	TEST_ASSERT_EQUAL(bundle6_get_mid_fragment_min_size(bundle), bundle_get_mid_fragment_min_size(bundle));
	TEST_ASSERT_EQUAL(bundle6_get_last_fragment_min_size(bundle), bundle_get_last_fragment_min_size(bundle));

	bundle->protocol_version = 7;

	TEST_ASSERT_EQUAL(bundle7_get_first_fragment_min_size(bundle), bundle_get_first_fragment_min_size(bundle));
	TEST_ASSERT_EQUAL(bundle7_get_last_fragment_min_size(bundle), bundle_get_mid_fragment_min_size(bundle));
	TEST_ASSERT_EQUAL(bundle7_get_last_fragment_min_size(bundle), bundle_get_last_fragment_min_size(bundle));

	bundle_free(bundle);

}

TEST(bundle, bundle_get_expiration_time_s)
{
	struct bundle *bundle = bundle_init();

	bundle->creation_timestamp_ms = 1;
	bundle->reception_timestamp_ms = 0;
	bundle->lifetime_ms = 86400;

	TEST_ASSERT_EQUAL(((1 + 86400 + 500) / 1000), bundle_get_expiration_time_s(bundle));

	bundle->creation_timestamp_ms = 0;
	TEST_ASSERT_EQUAL(0, bundle_get_expiration_time_s(bundle));

	struct bundle_block *block = bundle_block_create(BUNDLE_BLOCK_TYPE_BUNDLE_AGE);

	uint8_t bundle_age[1] = { 0x00 };

	block->number = 1;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = sizeof(bundle_age);
	block->data = malloc(sizeof(bundle_age));

	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, bundle_age, sizeof(bundle_age));

	struct bundle_block_list *blocks = bundle_block_entry_create(block);

	bundle->blocks = blocks;
	bundle->blocks->next = NULL;
	bundle->lifetime_ms = 0;
	TEST_ASSERT_EQUAL(0, bundle_get_expiration_time_s(bundle));
	bundle->lifetime_ms = 86400;

	uint64_t bundle_age_ms = 0;

	uint64_t res = ((86400 - bundle_age_ms + 0 + 500) / 1000);

	TEST_ASSERT_EQUAL(res, bundle_get_expiration_time_s(bundle));
}

TEST(bundle, bundle_age_update)
{
	struct bundle *bundle = bundle_init();

	TEST_ASSERT_NOT_NULL(bundle);
	TEST_ASSERT_EQUAL(UD3TN_OK, bundle_age_update(bundle, 0));

	bundle->creation_timestamp_ms = 1;
	bundle->reception_timestamp_ms = 0;
	bundle->lifetime_ms = 86400;

	struct bundle_block *block = bundle_block_create(BUNDLE_BLOCK_TYPE_BUNDLE_AGE);
	struct bundle_block_list *blocks = bundle_block_entry_create(block);

	bundle->blocks = blocks;
	bundle->blocks->next = NULL;
	TEST_ASSERT_EQUAL(UD3TN_FAIL, bundle_age_update(bundle, 0));

	uint8_t bundle_age[1] = { 0x00 };

	block->number = 1;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = sizeof(bundle_age);
	block->data = malloc(sizeof(bundle_age));

	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, bundle_age, sizeof(bundle_age));

	TEST_ASSERT_EQUAL(UD3TN_OK, bundle_age_update(bundle, 5));

}

TEST(bundle, bundle_get_unique_identifier)
{
	struct bundle *bundle = bundle_init();
	struct bundle_unique_identifier *bundle_id = malloc(sizeof(struct bundle_unique_identifier));

	bundle->source = strdup("ipn:243.350");

	struct bundle_block *block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);

	uint8_t payload[13] = {
		0x4c, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72,
		0x6c, 0x64, 0x21
	};

	block->number = 0;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));
	bundle->payload_block = block;

	bundle_id->creation_timestamp_ms = 0;
	bundle_id->protocol_version = 6;
	bundle_id->source = strdup("ipn:243.350");
	bundle_id->sequence_number = 0;
	bundle_id->fragment_offset = 0;
	bundle_id->payload_length = 13;

	struct bundle_unique_identifier bundle_id_2 = bundle_get_unique_identifier(bundle);

	TEST_ASSERT_EQUAL(bundle_id->creation_timestamp_ms, bundle_id_2.creation_timestamp_ms);
	TEST_ASSERT_EQUAL(bundle_id->protocol_version, bundle_id_2.protocol_version);
	TEST_ASSERT_EQUAL_STRING(bundle_id->source, bundle_id_2.source);
	TEST_ASSERT_EQUAL(bundle_id->sequence_number, bundle_id_2.sequence_number);
	TEST_ASSERT_EQUAL(bundle_id->fragment_offset, bundle_id_2.fragment_offset);
	TEST_ASSERT_EQUAL(bundle_id->payload_length, bundle_id_2.payload_length);

}

TEST(bundle, bundle_free_unique_identifier)
{
	struct bundle *bundle = bundle_init();
	struct bundle_unique_identifier *bundle_id = malloc(sizeof(struct bundle_unique_identifier));

	bundle->source = strdup("ipn:243.350");
	bundle_id->creation_timestamp_ms = 0;
	bundle_id->protocol_version = 6;
	bundle_id->source = strdup("ipn:243.350");
	bundle_id->sequence_number = 0;
	bundle_id->fragment_offset = 0;
	bundle_id->payload_length = 13;

	bundle_free_unique_identifier(bundle_id);
}

TEST(bundle, bundle_is_equal)
{
	struct bundle *bundle = bundle_init();
	struct bundle_unique_identifier *bundle_id = malloc(sizeof(struct bundle_unique_identifier));

	struct bundle_block *block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);

	uint8_t payload[13] = {
		0x4c, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72,
		0x6c, 0x64, 0x21
	};

	block->number = 0;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));
	bundle->payload_block = block;

	bundle->source = strdup("ipn:243.350");
	bundle_id->creation_timestamp_ms = 0;
	bundle_id->protocol_version = 6;
	bundle_id->source = strdup("ipn:243.350");
	bundle_id->sequence_number = 0;
	bundle_id->fragment_offset = 0;
	bundle_id->payload_length = 13;

	TEST_ASSERT_TRUE(bundle_is_equal_parent(bundle, bundle_id));

	bundle_id->sequence_number = 1;
	TEST_ASSERT_FALSE(bundle_is_equal_parent(bundle, bundle_id));

	bundle_id->sequence_number = 0;
	TEST_ASSERT_TRUE(bundle_is_equal(bundle, bundle_id));
	bundle_id->fragment_offset = 5;
	TEST_ASSERT_FALSE(bundle_is_equal(bundle, bundle_id));
}

TEST(bundle, bundle_adu_init)
{
	struct bundle *bundle = bundle_init();

	bundle->destination = strdup("dtn:GS2");
	bundle->source = strdup("ipn:243.350");

	struct bundle_block *block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);

	uint8_t payload[13] = {
		0x4c, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72,
		0x6c, 0x64, 0x21
	};

	block->number = 0;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));
	bundle->payload_block = block;

	struct bundle_adu adu = bundle_adu_init(bundle);

	TEST_ASSERT_EQUAL(0, adu.length);
	TEST_ASSERT_EQUAL(bundle->proc_flags, adu.proc_flags);
	TEST_ASSERT_EQUAL_STRING(bundle->destination, adu.destination);
	TEST_ASSERT_EQUAL(bundle->protocol_version, adu.protocol_version);
	TEST_ASSERT_EQUAL_STRING(bundle->source, adu.source);
}

TEST(bundle, bundle_to_adu)
{
	struct bundle *bundle = malloc(sizeof(struct bundle));

	TEST_ASSERT_NOT_NULL(bundle);

	bundle->protocol_version = 6;
	bundle->proc_flags = BUNDLE_FLAG_ACKNOWLEDGEMENT_REQUESTED;
	bundle->ret_constraints = BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED;

	bundle->destination = strdup("dtn:GS2");
	bundle->source = strdup("ipnd:243.350");
	bundle->report_to = strdup("dtn:none");
	bundle->current_custodian = strdup("dtn:GS2");

	bundle->crc_type = BUNDLE_CRC_TYPE_32;
	bundle->creation_timestamp_ms = 5;
	bundle->reception_timestamp_ms = 10;
	bundle->sequence_number = 15;
	bundle->lifetime_ms = 86400;
	bundle->fragment_offset = 24;
	bundle->total_adu_length = 3;
	bundle->primary_block_length = 8;

	struct bundle_block *block = malloc(sizeof(struct bundle_block));
	struct bundle_block_list *blocks = NULL;

	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	blocks = bundle_block_entry_create(block);
	bundle->blocks = blocks;
	uint8_t payload[6] = {
		0x82, 0x01, 0x63, 0x47, 0x53, 0x34
	};

	block->number = 2;
	block->crc_type = bundle->crc_type;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));

	bundle->payload_block = block;

	struct bundle_adu adu = bundle_to_adu(bundle);

	TEST_ASSERT_EQUAL_STRING(adu.destination, bundle->destination);
	TEST_ASSERT_EQUAL_STRING(adu.source, bundle->source);
	TEST_ASSERT_EQUAL(adu.protocol_version, bundle->protocol_version);
	TEST_ASSERT_EQUAL(adu.proc_flags, bundle->proc_flags);

	bundle_adu_free_members(adu);

}


TEST_GROUP_RUNNER(bundle)
{
	RUN_TEST_CASE(bundle, bundle_init);
	RUN_TEST_CASE(bundle, bundle_reset);
	RUN_TEST_CASE(bundle, bundle_free);
	RUN_TEST_CASE(bundle, bundle_drop);
	RUN_TEST_CASE(bundle, bundle_copy_headers);
	RUN_TEST_CASE(bundle, bundle_recalculate_header_length);
	RUN_TEST_CASE(bundle, bundle_dup);
	RUN_TEST_CASE(bundle, bundle_get_routing_priority);
	RUN_TEST_CASE(bundle, bundle_get_serialized_size);
	RUN_TEST_CASE(bundle, bundle_list_entry_create);
	RUN_TEST_CASE(bundle, bundle_list_entry_free);
	RUN_TEST_CASE(bundle, bundle_block_find_first_by_type);
	RUN_TEST_CASE(bundle, bundle_block_entry_create);
	RUN_TEST_CASE(bundle, bundle_block_entry_free);
	RUN_TEST_CASE(bundle, bundle_block_dup);
	RUN_TEST_CASE(bundle, bundle_block_entry_dup);
	RUN_TEST_CASE(bundle, bundle_block_list_dup);
	RUN_TEST_CASE(bundle, bundle_get_fragment_min_size);
	RUN_TEST_CASE(bundle, bundle_get_expiration_time_s);
	RUN_TEST_CASE(bundle, bundle_age_update);
	RUN_TEST_CASE(bundle, bundle_get_unique_identifier);
	RUN_TEST_CASE(bundle, bundle_free_unique_identifier);
	RUN_TEST_CASE(bundle, bundle_is_equal);
	RUN_TEST_CASE(bundle, bundle_adu_init);
	RUN_TEST_CASE(bundle, bundle_to_adu);
}
