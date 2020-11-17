#include "spp/spp.h"
#include "spp/spp_timecodes.h"

#include "ud3tn/common.h"

#include "unity_fixture.h"

#include <stdint.h>
#include <stddef.h>
#include <wchar.h>

TEST_GROUP(spp);

static struct spp_context_t *ctx;
static uint8_t buf[sizeof(wchar_t)*512];

TEST_SETUP(spp)
{
	ctx = spp_new_context();
	wmemset((wchar_t *)&buf[0],
			0xdeadbeef,
			ARRAY_SIZE(buf) / sizeof(wchar_t));
}

TEST_TEAR_DOWN(spp)
{
	spp_free_context(ctx);
	ctx = NULL;
}

TEST(spp, serialize_header_primary_only)
{
	static const uint8_t expected_header[6] = {
		0x11, 0x23,
		0x63, 0x42,
		0x00, 0x01
	};
	const struct spp_meta_t metadata = {
		.apid = 0x123,
		.is_request = true,
		.segment_number = 0x2342,
		.segment_status = SPP_SEGMENT_FIRST
	};
	uint8_t *out = &buf[0];

	int result = spp_serialize_header(ctx, &metadata, 2, &out);

	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_PTR(&buf[6], out);

	TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_header, &buf[0], 6);
}

TEST(spp, serialize_header_primary_with_timestamp)
{
	static const uint8_t expected_header[] = {
		0x19, 0x23,
		0x63, 0x42,
		0x00, 0x09,
		0x71, 0x68, 0x37, 0x0d, 0x00, 0x06, 0x76, 0xab,
	};
	const struct spp_meta_t metadata = {
		.apid = 0x123,
		.is_request = true,
		.segment_number = 0x2342,
		.segment_status = SPP_SEGMENT_FIRST,
		.dtn_timestamp = 577279245,
		.dtn_counter = 0x000676ab
	};
	uint8_t *out = &buf[0];

	struct spp_tc_context_t timecode;

	timecode.with_p_field = false;
	timecode.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	timecode.defaults.unsegmented.base_unit_octets = 4;
	timecode.defaults.unsegmented.fractional_octets = 4;

	spp_configure_timecode(ctx, &timecode);

	int result = spp_serialize_header(ctx, &metadata, 2, &out);

	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_PTR(&buf[14], out);

	TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_header, &buf[0],
			ARRAY_SIZE(expected_header));
}

TEST(spp, serialize_header_primary_with_ancillary_data)
{
	static const uint8_t expected_header[6] = {
		0x19, 0x23,
		0x63, 0x42,
		0x00, 0x02
	};
	const struct spp_meta_t metadata = {
		.apid = 0x123,
		.is_request = true,
		.segment_number = 0x2342,
		.segment_status = SPP_SEGMENT_FIRST
	};
	uint8_t *out = &buf[0];

	spp_configure_ancillary_data(ctx, 1);

	int result = spp_serialize_header(ctx, &metadata, 2, &out);

	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_PTR(&buf[6], out);

	TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_header, &buf[0], 6);
}

TEST(spp, configure_ancillary_data)
{
	const size_t default_size = spp_get_ancillary_data_length(ctx);

	TEST_ASSERT_EQUAL(0, default_size);

	const size_t expected_size = 10;

	spp_configure_ancillary_data(ctx, expected_size);

	const size_t configured_size = spp_get_ancillary_data_length(ctx);

	TEST_ASSERT_EQUAL(expected_size, configured_size);
}

TEST(spp, get_size_primary_header_only)
{
	const size_t size = spp_get_size(ctx, 10);

	TEST_ASSERT_EQUAL(16, size);
}

TEST(spp, get_size_with_ancillary_data)
{
	spp_configure_ancillary_data(ctx, 1);

	const size_t size = spp_get_size(ctx, 10);

	TEST_ASSERT_EQUAL(17, size);
}

TEST(spp, get_size_with_timestamp)
{
	struct spp_tc_context_t timecode;

	timecode.with_p_field = true;
	timecode.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	timecode.defaults.unsegmented.base_unit_octets = 1;
	timecode.defaults.unsegmented.fractional_octets = 0;

	spp_configure_timecode(ctx, &timecode);

	const size_t size = spp_get_size(ctx, 10);

	TEST_ASSERT_EQUAL(18, size);
}

TEST(spp, get_min_payload_size)
{
	const size_t min_size = spp_get_min_payload_size(ctx);

	TEST_ASSERT_EQUAL(1, min_size);
}

TEST(spp, get_min_payload_size_with_ancillary_data)
{
	spp_configure_ancillary_data(ctx, 1);

	const size_t min_size = spp_get_min_payload_size(ctx);

	TEST_ASSERT_EQUAL(0, min_size);
}

TEST(spp, get_min_payload_size_with_timestamp)
{
	struct spp_tc_context_t timecode;

	timecode.with_p_field = true;
	timecode.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	timecode.defaults.unsegmented.base_unit_octets = 1;
	timecode.defaults.unsegmented.fractional_octets = 0;

	spp_configure_timecode(ctx, &timecode);

	const size_t min_size = spp_get_min_payload_size(ctx);

	TEST_ASSERT_EQUAL(0, min_size);
}

TEST(spp, get_max_payload_size)
{
	const size_t max_size = spp_get_max_payload_size(ctx);

	TEST_ASSERT_EQUAL(65536, max_size);
}

TEST(spp, get_max_payload_size_with_ancillary_data)
{
	spp_configure_ancillary_data(ctx, 10);

	const size_t max_size = spp_get_max_payload_size(ctx);

	TEST_ASSERT_EQUAL(65526, max_size);
}

TEST(spp, get_max_payload_size_with_timestamp)
{
	struct spp_tc_context_t timecode;

	timecode.with_p_field = true;
	timecode.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	timecode.defaults.unsegmented.base_unit_octets = 1;
	timecode.defaults.unsegmented.fractional_octets = 0;

	spp_configure_timecode(ctx, &timecode);

	const size_t max_size = spp_get_max_payload_size(ctx);

	TEST_ASSERT_EQUAL(65534, max_size);
}

TEST_GROUP_RUNNER(spp)
{
	RUN_TEST_CASE(spp, serialize_header_primary_only);
	RUN_TEST_CASE(spp, serialize_header_primary_with_ancillary_data);
	RUN_TEST_CASE(spp, serialize_header_primary_with_timestamp);
	RUN_TEST_CASE(spp, configure_ancillary_data);
	RUN_TEST_CASE(spp, get_size_primary_header_only);
	RUN_TEST_CASE(spp, get_size_with_ancillary_data);
	RUN_TEST_CASE(spp, get_size_with_timestamp);
	RUN_TEST_CASE(spp, get_min_payload_size);
	RUN_TEST_CASE(spp, get_min_payload_size_with_ancillary_data);
	RUN_TEST_CASE(spp, get_min_payload_size_with_timestamp);
	RUN_TEST_CASE(spp, get_max_payload_size);
	RUN_TEST_CASE(spp, get_max_payload_size_with_ancillary_data);
	RUN_TEST_CASE(spp, get_max_payload_size_with_timestamp);
}
