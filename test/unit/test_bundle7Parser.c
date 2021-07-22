/**
 * Unit Test for BPbis parser
 *
 * Raw CBOR data is loaded from "test_bundle7Data.c".
 */

#include "bundle7/eid.h"
#include "bundle7/parser.h"
#include "bundle7/reports.h"
#include "bundle7/hopcount.h"
#include "bundle7/bundle_age.h"

#include "ud3tn/bundle.h"
#include "ud3tn/report_manager.h"

#include "unity_fixture.h"

#include <string.h>


TEST_GROUP(bundle7Parser);

extern uint8_t cbor_simple_bundle[];
extern size_t len_simple_bundle;

static struct bundle *bundle;

static void send_callback(struct bundle *_bundle, void *param)
{
	(void)param;
	bundle = _bundle;
}


TEST_SETUP(bundle7Parser)
{
	bundle = NULL;
}


TEST_TEAR_DOWN(bundle7Parser)
{
	if (bundle != NULL) {
		bundle_free(bundle);
		bundle = NULL;
	}
}


TEST(bundle7Parser, eid_parser)
{
	char *eid;
	const uint8_t sat_1[] = { 0x82, 0x01, 0x64, 0x53, 0x41, 0x54, 0x31 };
	const uint8_t ipn[] = { 0x82, 0x02, 0x82, 0x18, 0x2a, 0x18, 0x48 };
	const uint8_t none[] = { 0x82, 0x01, 0x00 };

	eid = bundle7_eid_parse(sat_1, sizeof(sat_1));
	TEST_ASSERT_NOT_NULL(eid);
	TEST_ASSERT_EQUAL_STRING("dtn:SAT1", eid);

	eid = bundle7_eid_parse(ipn, sizeof(ipn));
	TEST_ASSERT_NOT_NULL(eid);
	TEST_ASSERT_EQUAL_STRING("ipn:42.72", eid);

	eid = bundle7_eid_parse(none, sizeof(none));
	TEST_ASSERT_NOT_NULL(eid);
	TEST_ASSERT_EQUAL_STRING("dtn:none", eid);
}

static void parse_bundle_chunked(const size_t chunk_size)
{
	struct bundle7_parser state;
	struct parser *parser = bundle7_parser_init(
		&state,
		&send_callback,
		NULL
	);

	TEST_ASSERT_NOT_NULL(parser);

	size_t read = 0;
	size_t chunk = chunk_size;

	while (read < len_simple_bundle
			&& state.basedata->status == PARSER_STATUS_GOOD) {
		if (state.basedata->flags & PARSER_FLAG_BULK_READ) {
			TEST_ASSERT_TRUE(read + state.basedata->next_bytes
				< len_simple_bundle);

			// Bulk read operation
			memcpy(state.basedata->next_buffer,
				cbor_simple_bundle + read,
				state.basedata->next_bytes);

			// Advance parsing pointer
			read += state.basedata->next_bytes;

			// Deactivate bulk read mode
			state.basedata->flags &= ~PARSER_FLAG_BULK_READ;
		} else {
			if (chunk + read >= len_simple_bundle)
				chunk = len_simple_bundle - read;

			size_t parsed = bundle7_parser_read(&state,
					cbor_simple_bundle + read,
					chunk);

			if (parsed == 0) {
				chunk *= 2;
			} else {
				chunk = chunk_size;
				read += parsed;
			}
		}

		TEST_ASSERT_NOT_EQUAL(state.basedata->status,
			PARSER_STATUS_ERROR);
	}

	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, state.basedata->status);
	TEST_ASSERT_FALSE(state.basedata->flags & PARSER_FLAG_CRC_INVALID);
	TEST_ASSERT_NULL(state.bundle);
	TEST_ASSERT_NOT_NULL(bundle);

	// Creation Timestamp 2020-11-12T09:51:03
	TEST_ASSERT_EQUAL(658489863000, bundle->creation_timestamp_ms);

	// Primary block CRC
	TEST_ASSERT_EQUAL(BUNDLE_CRC_TYPE_NONE, bundle->crc_type);

	// Bundle Processing flags
	TEST_ASSERT_TRUE(bundle->proc_flags
		& BUNDLE_FLAG_MUST_NOT_BE_FRAGMENTED);
	TEST_ASSERT_TRUE(bundle->proc_flags
		& BUNDLE_FLAG_REPORT_DELIVERY);
	TEST_ASSERT_FALSE(bundle->proc_flags
		& BUNDLE_FLAG_IS_FRAGMENT);
	TEST_ASSERT_FALSE(bundle->proc_flags
		& BUNDLE_FLAG_ADMINISTRATIVE_RECORD);
	TEST_ASSERT_FALSE(bundle->proc_flags
		& BUNDLE_FLAG_ACKNOWLEDGEMENT_REQUESTED);
	TEST_ASSERT_FALSE(bundle->proc_flags
		& BUNDLE_V6_FLAG_NORMAL_PRIORITY);
	TEST_ASSERT_FALSE(bundle->proc_flags
		& BUNDLE_V6_FLAG_EXPEDITED_PRIORITY);
	TEST_ASSERT_FALSE(bundle->proc_flags
		& BUNDLE_FLAG_REPORT_RECEPTION);
	TEST_ASSERT_FALSE(bundle->proc_flags
		& BUNDLE_FLAG_REPORT_FORWARDING);
	TEST_ASSERT_FALSE(bundle->proc_flags
		& BUNDLE_FLAG_REPORT_DELETION);
	TEST_ASSERT_FALSE(bundle->proc_flags
		& BUNDLE_FLAG_REPORT_STATUS_TIME);

	struct bundle_block_list *block_element = bundle->blocks;

	// Previous node block
	TEST_ASSERT_EQUAL(BUNDLE_BLOCK_TYPE_PREVIOUS_NODE,
		block_element->data->type);
	TEST_ASSERT_EQUAL(BUNDLE_BLOCK_FLAG_NONE,
		block_element->data->flags);
	TEST_ASSERT_EQUAL(6, block_element->data->length);

	uint8_t previous_node[6] = {
		0x82, 0x01, 0x63, 0x47, 0x53, 0x34
	};
	TEST_ASSERT_EQUAL_INT8_ARRAY(previous_node,
		block_element->data->data, 6);
	TEST_ASSERT_EQUAL(BUNDLE_CRC_TYPE_NONE, block_element->data->crc_type);

	// Hop count block
	block_element = block_element->next;
	TEST_ASSERT_EQUAL(BUNDLE_BLOCK_TYPE_HOP_COUNT,
		block_element->data->type);
	TEST_ASSERT_EQUAL(4, block_element->data->length);

	uint8_t hop_count[4] = {
		0x82, 0x18, 0x1e, 0x00
	};
	TEST_ASSERT_EQUAL_INT8_ARRAY(hop_count, block_element->data->data, 4);
	TEST_ASSERT_EQUAL(BUNDLE_CRC_TYPE_NONE, block_element->data->crc_type);

	// Bundle age block
	block_element = block_element->next;
	TEST_ASSERT_EQUAL(BUNDLE_BLOCK_TYPE_BUNDLE_AGE,
		block_element->data->type);
	TEST_ASSERT_EQUAL(1, block_element->data->length);

	uint8_t bundle_age[1] = { 0x00 };

	TEST_ASSERT_EQUAL_INT8_ARRAY(bundle_age, block_element->data->data, 1);

	// Payload
	block_element = block_element->next;
	TEST_ASSERT_EQUAL_PTR(block_element->data,
		bundle->payload_block);

	// Payload block must be the last block
	TEST_ASSERT_NULL(block_element->next);

	uint8_t payload[] = {
		'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!',
	};

	TEST_ASSERT_EQUAL(sizeof(payload), bundle->payload_block->length);
	TEST_ASSERT_EQUAL_INT8_ARRAY(payload,
		bundle->payload_block->data, 12);
	TEST_ASSERT_EQUAL(BUNDLE_CRC_TYPE_NONE, block_element->data->crc_type);

	TEST_ASSERT_EQUAL(len_simple_bundle,
			  bundle_get_serialized_size(bundle));

	// Assert that serialized length is correct after recalculation
	bundle_recalculate_header_length(bundle);

	TEST_ASSERT_EQUAL(len_simple_bundle,
			  bundle_get_serialized_size(bundle));
}

TEST(bundle7Parser, bundle_parser)
{
	for (size_t chunk = 1; chunk < len_simple_bundle; ++chunk) {
		parse_bundle_chunked(chunk);

		// Clear bundle
		bundle_free(bundle);
		bundle = NULL;
	}
}

// -------------------------
// CRC 16 AX.25 Verification
// -------------------------
//
extern uint8_t cbor_crc16_primary_block[];
extern uint8_t cbor_crc16_payload_block[];
extern size_t len_crc16_primary_block;
extern size_t len_crc16_payload_block;

TEST(bundle7Parser, crc16_verification)
{
	struct bundle7_parser state;
	struct parser *parser = bundle7_parser_init(
		&state,
		&send_callback,
		NULL
	);

	TEST_ASSERT_NOT_NULL(parser);

	//
	// CRC 16 primary block checksum
	// -----------------------------
	//
	size_t parsed = bundle7_parser_read(&state, cbor_crc16_primary_block,
		len_crc16_primary_block);

	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, state.basedata->status);
	TEST_ASSERT_FALSE(state.basedata->flags & PARSER_FLAG_CRC_INVALID);
	TEST_ASSERT_EQUAL(parsed, len_crc16_primary_block);
	TEST_ASSERT_NULL(state.bundle);
	TEST_ASSERT_NOT_NULL(bundle);

	TEST_ASSERT_EQUAL(bundle->crc.checksum, 0x7123);

	// Clear bundle
	bundle_free(bundle);
	bundle = NULL;

	//
	// CRC 16 payload block checksum
	// -----------------------------
	//
	bundle7_parser_reset(&state);

	parsed = bundle7_parser_read(&state, cbor_crc16_payload_block,
		len_crc16_payload_block);

	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, state.basedata->status);
	TEST_ASSERT_FALSE(state.basedata->flags & PARSER_FLAG_CRC_INVALID);
	TEST_ASSERT_EQUAL(parsed, len_crc16_payload_block);
	TEST_ASSERT_NULL(state.bundle);
	TEST_ASSERT_NOT_NULL(bundle);

	TEST_ASSERT_EQUAL(bundle->payload_block->crc.checksum, 0x60d7);

	// Clear bundle
	bundle_free(bundle);
	bundle = NULL;
}

// ------------------------------
// CRC 32 Castagnoli Verification
// ------------------------------
//
extern uint8_t cbor_crc32_primary_block[];
extern uint8_t cbor_crc32_payload_block[];
extern size_t len_crc32_primary_block;
extern size_t len_crc32_payload_block;

TEST(bundle7Parser, crc32_verification)
{
	struct bundle7_parser state;
	struct parser *parser = bundle7_parser_init(
		&state,
		&send_callback,
		NULL
	);

	TEST_ASSERT_NOT_NULL(parser);

	//
	// CRC 32 primary block checksum
	// -----------------------------
	//
	size_t parsed = bundle7_parser_read(&state, cbor_crc32_primary_block,
		len_crc32_primary_block);

	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, state.basedata->status);
	TEST_ASSERT_FALSE(state.basedata->flags & PARSER_FLAG_CRC_INVALID);
	TEST_ASSERT_EQUAL(parsed, len_crc32_primary_block);
	TEST_ASSERT_NULL(state.bundle);
	TEST_ASSERT_NOT_NULL(bundle);

	TEST_ASSERT_EQUAL(bundle->crc.checksum, 0x9542defd);

	// Clear bundle
	bundle_free(bundle);
	bundle = NULL;

	//
	// CRC 32 payload block checksum
	// -----------------------------
	//
	bundle7_parser_reset(&state);

	parsed = bundle7_parser_read(&state, cbor_crc32_payload_block,
		len_crc32_payload_block);

	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, state.basedata->status);
	TEST_ASSERT_FALSE(state.basedata->flags & PARSER_FLAG_CRC_INVALID);
	TEST_ASSERT_EQUAL(parsed, len_crc32_payload_block);
	TEST_ASSERT_NULL(state.bundle);
	TEST_ASSERT_NOT_NULL(bundle);

	TEST_ASSERT_EQUAL(bundle->payload_block->crc.checksum, 0xc3aec552);

	// Clear bundle
	bundle_free(bundle);
	bundle = NULL;
}

// --------------------
// Invalid CRC Handling
// --------------------

extern uint8_t cbor_invalid_crc16[];
extern size_t len_invalid_crc16;

TEST(bundle7Parser, invalid_crc_handling)
{
	struct bundle7_parser state;
	struct parser *parser = bundle7_parser_init(
		&state,
		&send_callback,
		NULL
	);

	TEST_ASSERT_NOT_NULL(parser);

	size_t parsed = bundle7_parser_read(&state, cbor_invalid_crc16,
		len_invalid_crc16);

	// The bundle must be parsed completly
	TEST_ASSERT_EQUAL(PARSER_STATUS_DONE, state.basedata->status);
	TEST_ASSERT_EQUAL(parsed, len_invalid_crc16);

	// but the CRC_INVALID flag must be set
	TEST_ASSERT_TRUE(state.basedata->flags & PARSER_FLAG_CRC_INVALID);

	// and the bundle must not be send callback must not be called
	TEST_ASSERT_NULL(state.bundle);
	TEST_ASSERT_NULL(bundle);
}

static const uint8_t cbor_status_report[] = {
	// [
	//   1,                   // Record type code
	//     [
	//       [                // bundle status information
	//         [true, 10],    //   Reporting node received bundle
	//         [false],       //   Reporting node forwarded the bundle
	//         [false],       //   Reporting node delivered the bundle
	//         [false]        //   Reporting node deleted the bundle
	//       ],
	//       0,                // bundle status report reason code
	//       [1, 0],           // Source node ID of reported bundle
	//       [2165, 12]        // Creation time of the reported bundle
	//     ]
	//   ]
	// ]
	0x82, 0x01, 0x84, 0x84, 0x82, 0xf5, 0x0a, 0x81, 0xf4, 0x81,
	0xf4, 0x81, 0xf4, 0x00, 0x82, 0x01, 0x00, 0x82, 0x19, 0x08, 0x75, 0x0c
};

TEST(bundle7Parser, status_report_parser)
{
	struct bundle *bundle = bundle_init();

	TEST_ASSERT_NOT_NULL(bundle);

	// Primary Block
	bundle->proc_flags = BUNDLE_FLAG_REPORT_STATUS_TIME
		| BUNDLE_FLAG_ADMINISTRATIVE_RECORD;
	bundle->creation_timestamp_ms = 4200;
	bundle->sequence_number = 0;
	bundle->lifetime_ms = 86400;

	struct bundle_block *payload;
	struct bundle_block_list *entry;

	// Payload
	payload = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	entry = bundle_block_entry_create(payload);

	payload->length = sizeof(cbor_status_report);
	payload->data = malloc(payload->length);
	memcpy(payload->data, cbor_status_report, sizeof(cbor_status_report));

	bundle->blocks = entry;
	bundle->payload_block = payload;

	// Parser payload
	struct bundle_administrative_record *record =
		bundle7_parse_administrative_record(
			bundle->payload_block->data,
			bundle->payload_block->length
		);

	TEST_ASSERT_NOT_NULL(record);
	TEST_ASSERT_NOT_NULL(record->status_report);
	TEST_ASSERT_EQUAL(BUNDLE_AR_STATUS_REPORT, record->type);
	TEST_ASSERT_EQUAL(0, record->flags);

	TEST_ASSERT_EQUAL(2165, record->bundle_creation_timestamp_ms);
	TEST_ASSERT_EQUAL(12, record->bundle_sequence_number);

	TEST_ASSERT_EQUAL(10, record->status_report->bundle_received_time);
	TEST_ASSERT_EQUAL(BUNDLE_SR_FLAG_BUNDLE_RECEIVED,
		record->status_report->status);
	TEST_ASSERT_EQUAL(BUNDLE_SR_REASON_NO_INFO,
		record->status_report->reason);

	// free_administrative_record(record);
	bundle_free(bundle);
}


// [30, 4]
static const uint8_t cbor_hop_count[] = { 0x82, 0x18, 0x1e, 0x04 };

TEST(bundle7Parser, hop_count)
{
	struct bundle_hop_count hop_count;

	// Try to parse empty buffer
	TEST_ASSERT_FALSE(bundle7_hop_count_parse(&hop_count, NULL, 0));

	// Try to parse wrong CBOR structure
	TEST_ASSERT_FALSE(bundle7_hop_count_parse(&hop_count,
		cbor_status_report, sizeof(cbor_status_report)));

	// Parse valid CBOR structure
	TEST_ASSERT_TRUE(bundle7_hop_count_parse(&hop_count,
		cbor_hop_count, sizeof(cbor_hop_count)));

	TEST_ASSERT_EQUAL(30, hop_count.limit);
	TEST_ASSERT_EQUAL(4, hop_count.count);
}


// 42000
static const uint8_t cbor_bundle_age[] = { 0x19, 0xa4, 0x10 };

TEST(bundle7Parser, bundle_age)
{
	uint64_t bundle_age;

	// Try to parse empty buffer
	TEST_ASSERT_FALSE(bundle_age_parse(&bundle_age, NULL, 0));

	// Try to parse wrong CBOR structure
	TEST_ASSERT_FALSE(bundle_age_parse(&bundle_age,
		cbor_status_report, sizeof(cbor_status_report)));

	// Parse valid CBOR structure
	TEST_ASSERT_TRUE(bundle_age_parse(&bundle_age,
		cbor_bundle_age, sizeof(cbor_bundle_age)));

	TEST_ASSERT_EQUAL(42000, bundle_age);
}


TEST_GROUP_RUNNER(bundle7Parser)
{
	RUN_TEST_CASE(bundle7Parser, eid_parser);
	RUN_TEST_CASE(bundle7Parser, bundle_parser);
	RUN_TEST_CASE(bundle7Parser, crc16_verification);
	RUN_TEST_CASE(bundle7Parser, crc32_verification);
	RUN_TEST_CASE(bundle7Parser, invalid_crc_handling);
	RUN_TEST_CASE(bundle7Parser, status_report_parser);
	RUN_TEST_CASE(bundle7Parser, hop_count);
	RUN_TEST_CASE(bundle7Parser, bundle_age);
}
