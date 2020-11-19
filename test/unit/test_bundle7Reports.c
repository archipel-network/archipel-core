#include "bundle7/reports.h"

#include "ud3tn/bundle.h"

#include "unity_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


TEST_GROUP(bundle7Reports);

static struct bundle *bundle;


static struct bundle *create_bundle(void)
{
	struct bundle *bundle = bundle_init();

	TEST_ASSERT_NOT_NULL(bundle);

	bundle->protocol_version = 7;
	bundle->proc_flags |= BUNDLE_FLAG_REPORT_STATUS_TIME;

	bundle->destination = strdup("dtn:GS1");
	bundle->source = strdup("ipn:243.350");
	bundle->report_to = strdup("dtn:GS2");

	struct bundle_block_list *entry;
	struct bundle_block *block;

	// Payload
	block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	entry = bundle_block_entry_create(block);

	bundle->blocks = entry;
	bundle->payload_block = block;

	uint8_t payload[] = {
		0x4c, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72,
		0x6c, 0x64, 0x21
	};

	block->number = 0;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = sizeof(payload);
	block->data = malloc(sizeof(payload));
	TEST_ASSERT_NOT_NULL(block->data);
	memcpy(block->data, payload, sizeof(payload));

	return bundle;
}


TEST_SETUP(bundle7Reports)
{
	bundle = NULL;
}


TEST_TEAR_DOWN(bundle7Reports)
{
	if (bundle != NULL)
		bundle_free(bundle);
}


TEST(bundle7Reports, generate_status_reports)
{
	bundle = create_bundle();

	struct bundle_status_report report = {
		// Status
		.status = BUNDLE_SR_FLAG_BUNDLE_RECEIVED
			| BUNDLE_SR_FLAG_BUNDLE_FORWARDED,

		// Times
		.bundle_received_time = 100,
		.bundle_forwarded_time = 200,

		.reason = BUNDLE_SR_REASON_NO_INFO,
	};

	struct bundle *record = bundle7_generate_status_report(bundle,
		&report, "dtn:test");

	TEST_ASSERT_NOT_NULL(record);

	// [
	//   1,                  # Administative Record Type
	//   [
	//     [                 # Bundle Status Information
	//       [true, 100],
	//       [true, 200],
	//       [false],
	//       [false]
	//     ],
	//     0,                # Reason Code
	//     [2, [243, 350]],  # Source EID
	//     [0, 0]            # Creation Timestamp
	//   ]
	// ]
	const uint8_t cbor_report[] = {
		0x82, 0x01, 0x84, 0x84, 0x82, 0xf5, 0x18, 0x64,
		0x82, 0xf5, 0x18, 0xc8, 0x81, 0xf4, 0x81, 0xf4, 0x00, 0x82,
		0x02, 0x82, 0x18, 0xf3, 0x19, 0x01, 0x5e, 0x82, 0x00, 0x00,
	};

	TEST_ASSERT_TRUE(record->proc_flags
		& BUNDLE_FLAG_ADMINISTRATIVE_RECORD);

	TEST_ASSERT_EQUAL_STRING("dtn:GS2", record->destination);
	TEST_ASSERT_EQUAL_STRING("dtn:test", record->source);
	TEST_ASSERT_EQUAL(sizeof(cbor_report), record->payload_block->length);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(
		cbor_report,
		record->payload_block->data,
		sizeof(cbor_report)
	);

	bundle_free(record);
}


TEST_GROUP_RUNNER(bundle7Reports)
{
	RUN_TEST_CASE(bundle7Reports, generate_status_reports);
}
