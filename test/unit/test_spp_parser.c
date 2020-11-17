#include "spp/spp_parser.h"

#include "ud3tn/common.h"

#include "unity_fixture.h"

TEST_GROUP(spp_parser);

static struct spp_context_t *ctx;
static struct spp_parser parser_instance;

TEST_SETUP(spp_parser)
{
	ctx = spp_new_context();
	spp_parser_init(&parser_instance, ctx);
}

TEST_TEAR_DOWN(spp_parser)
{
	spp_free_context(ctx);
	ctx = NULL;
}

TEST(spp_parser, parse_header)
{
	const uint8_t packet[] = {
		0x00, 0x01,
		0x80, 0x03,
		0x00, 0x01,
		0x23, 0x42
	};

	const size_t read = spp_parser_read(
				&parser_instance,
				&packet[0],
			ARRAY_SIZE(packet));

	TEST_ASSERT_EQUAL(6, read);

	TEST_ASSERT_EQUAL(SPP_SEGMENT_LAST,
			  parser_instance.header.segment_status);
	TEST_ASSERT_EQUAL(1, parser_instance.header.apid);
	TEST_ASSERT_EQUAL(false, parser_instance.header.is_request);
	TEST_ASSERT_EQUAL(false, parser_instance.header.has_secondary_header);
	TEST_ASSERT_EQUAL(3, parser_instance.header.segment_number);
	TEST_ASSERT_EQUAL(2, parser_instance.header.data_length);

	TEST_ASSERT_EQUAL(SPP_PARSER_STATE_DATA_SUBPARSER,
			  parser_instance.state);
}

TEST(spp_parser, parse_header_bytewise)
{
	const uint8_t packet[] = {
		0x00, 0x01,
		0x80, 0x03,
		0x00, 0x01,
		0x23, 0x42
	};

	for (unsigned int i = 0; i < 6; ++i) {
		const size_t read = spp_parser_read(
					&parser_instance,
					&packet[i], 1);

		TEST_ASSERT_EQUAL(1, read);
	}

	TEST_ASSERT_EQUAL(SPP_SEGMENT_LAST,
			  parser_instance.header.segment_status);
	TEST_ASSERT_EQUAL(1, parser_instance.header.apid);
	TEST_ASSERT_EQUAL(false, parser_instance.header.is_request);
	TEST_ASSERT_EQUAL(false, parser_instance.header.has_secondary_header);
	TEST_ASSERT_EQUAL(3, parser_instance.header.segment_number);
	TEST_ASSERT_EQUAL(2, parser_instance.header.data_length);

	TEST_ASSERT_EQUAL(SPP_PARSER_STATE_DATA_SUBPARSER,
			  parser_instance.state);
}

TEST(spp_parser, parse_header_with_timestamp)
{
	const uint8_t packet[] = {
		0x08, 0x01,
		0x80, 0x03,
		0x00, 0x09,
		0x71, 0x68, 0x37, 0x0d, 0x00, 0x06, 0x76, 0xab,
		0x23, 0x42,
	};

	struct spp_tc_context_t timecode;
	struct spp_meta_t meta;

	timecode.with_p_field = false;
	timecode.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	timecode.defaults.unsegmented.base_unit_octets = 4;
	timecode.defaults.unsegmented.fractional_octets = 4;

	TEST_ASSERT_TRUE(spp_configure_timecode(ctx, &timecode));

	const size_t read = spp_parser_read(
				&parser_instance,
				&packet[0],
			ARRAY_SIZE(packet));

	TEST_ASSERT_EQUAL(14, read);

	TEST_ASSERT_TRUE(spp_parser_get_meta(&parser_instance,
					     &meta));

	TEST_ASSERT_EQUAL(SPP_SEGMENT_LAST,
			  meta.segment_status);
	TEST_ASSERT_EQUAL(1, meta.apid);
	TEST_ASSERT_EQUAL(false, meta.is_request);
	TEST_ASSERT_EQUAL(3, meta.segment_number);

	size_t data_length = 0;

	TEST_ASSERT_TRUE(spp_parser_get_data_length(&parser_instance,
						    &data_length));

	TEST_ASSERT_EQUAL(2, data_length);

	TEST_ASSERT_EQUAL_UINT64(577279245,
				 parser_instance.dtn_timestamp);

	TEST_ASSERT_EQUAL(SPP_PARSER_STATE_DATA_SUBPARSER,
			  parser_instance.state);
}

TEST(spp_parser, parse_header_with_timestamp_segmentwise)
{
	const uint8_t packet[] = {
		0x08, 0x01,
		0x80, 0x03,
		0x00, 0x09,
		0x71, 0x68, 0x37, 0x0d, 0x00, 0x06, 0x76, 0xab,
		0x23, 0x42,
	};

	struct spp_tc_context_t timecode;
	struct spp_meta_t meta;

	timecode.with_p_field = false;
	timecode.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	timecode.defaults.unsegmented.base_unit_octets = 4;
	timecode.defaults.unsegmented.fractional_octets = 4;

	TEST_ASSERT_TRUE(spp_configure_timecode(ctx, &timecode));

	unsigned int i = 0;

	for (; i < 6; ++i) {
		const size_t read = spp_parser_read(
					&parser_instance,
					&packet[i], 1);

		TEST_ASSERT_EQUAL(1, read);
		TEST_ASSERT_FALSE(spp_parser_get_meta(&parser_instance,
						      NULL));
		TEST_ASSERT_FALSE(spp_parser_get_data_length(&parser_instance,
							     NULL));
	}

	for (; i < 13; ++i) {
		const size_t read = spp_parser_read(
					&parser_instance,
					&packet[i], 1);

		TEST_ASSERT_EQUAL(1, read);
		TEST_ASSERT_FALSE(spp_parser_get_meta(&parser_instance,
						      NULL));
		TEST_ASSERT_FALSE(spp_parser_get_data_length(&parser_instance,
							     NULL));
	}

	const size_t read = spp_parser_read(
				&parser_instance,
				&packet[i], 1);

	TEST_ASSERT_EQUAL(1, read);

	TEST_ASSERT_TRUE(spp_parser_get_meta(&parser_instance,
					     &meta));

	TEST_ASSERT_EQUAL(SPP_SEGMENT_LAST,
			  meta.segment_status);
	TEST_ASSERT_EQUAL(1, meta.apid);
	TEST_ASSERT_EQUAL(false, meta.is_request);
	TEST_ASSERT_EQUAL(3, meta.segment_number);

	size_t data_length = 0;

	TEST_ASSERT_TRUE(spp_parser_get_data_length(&parser_instance,
						    &data_length));

	TEST_ASSERT_EQUAL(2, data_length);

	TEST_ASSERT_EQUAL_UINT64(577279245,
				 parser_instance.dtn_timestamp);

	TEST_ASSERT_EQUAL(SPP_PARSER_STATE_DATA_SUBPARSER,
			  parser_instance.state);
}

TEST_GROUP_RUNNER(spp_parser)
{
	RUN_TEST_CASE(spp_parser, parse_header);
	RUN_TEST_CASE(spp_parser, parse_header_bytewise);
	RUN_TEST_CASE(spp_parser, parse_header_with_timestamp);
	RUN_TEST_CASE(spp_parser, parse_header_with_timestamp_segmentwise);
}
