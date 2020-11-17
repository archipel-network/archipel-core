#include "spp/spp_timecodes.h"

#include "ud3tn/common.h"

#include "unity_fixture.h"


TEST_GROUP(spp_timecodes);

TEST_SETUP(spp_timecodes)
{

}

TEST_TEAR_DOWN(spp_timecodes)
{

}

TEST(spp_timecodes, parse_unsegmented_without_p_no_fractional)
{
	struct spp_tc_context_t ctx;

	ctx.with_p_field = false;
	ctx.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	ctx.defaults.unsegmented.base_unit_octets = 4;
	ctx.defaults.unsegmented.fractional_octets = 0;

	struct spp_tc_parser_t parser;

	spp_tc_parser_init(&ctx, &parser);

	static const uint8_t bytes[] = {
		0x4e, 0xff, 0xa2, 0x17,
	};

	for (unsigned int i = 0; i < 3; ++i) {
		int result = spp_tc_parser_feed(&parser, bytes[i]);

		TEST_ASSERT_EQUAL(SPP_TC_PARSER_GOOD, result);
	}

	int result = spp_tc_parser_feed(&parser, bytes[3]);

	TEST_ASSERT_EQUAL(SPP_TC_PARSER_DONE, result);

	const uint64_t dtn_timestamp = spp_tc_get_dtn_timestamp(&parser);

	TEST_ASSERT_EQUAL_UINT64(23, dtn_timestamp);
}

TEST(spp_timecodes, parse_unsegmented_without_p_with_fractional)
{
	struct spp_tc_context_t ctx;

	ctx.with_p_field = false;
	ctx.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	ctx.defaults.unsegmented.base_unit_octets = 4;
	ctx.defaults.unsegmented.fractional_octets = 2;

	struct spp_tc_parser_t parser;

	spp_tc_parser_init(&ctx, &parser);

	static const uint8_t bytes[] = {
		0x4e, 0xff, 0xa2, 0x17, 0x23, 0x42,
	};

	for (unsigned int i = 0; i < 5; ++i) {
		int result = spp_tc_parser_feed(&parser, bytes[i]);

		TEST_ASSERT_EQUAL(SPP_TC_PARSER_GOOD, result);
	}

	int result = spp_tc_parser_feed(&parser, bytes[3]);

	TEST_ASSERT_EQUAL(SPP_TC_PARSER_DONE, result);

	const uint64_t dtn_timestamp = spp_tc_get_dtn_timestamp(&parser);

	TEST_ASSERT_EQUAL_UINT64(23, dtn_timestamp);
}

TEST(spp_timecodes, parse_unsegmented_with_p_fractional)
{
	struct spp_tc_context_t ctx;

	ctx.with_p_field = true;

	struct spp_tc_parser_t parser;

	spp_tc_parser_init(&ctx, &parser);

	static const uint8_t bytes[] = {
		0x1e,  /* preamble */
		0x4e, 0xff, 0xa2, 0x17, 0x23, 0x42,
	};

	{
		int result = spp_tc_parser_feed(&parser, bytes[0]);

		TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_GOOD, result,
					  "p field");
	}

	for (unsigned int i = 1; i < ARRAY_SIZE(bytes)-1; ++i) {
		int result = spp_tc_parser_feed(&parser, bytes[i]);

		TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_GOOD, result,
					  "tc data");
	}

	int result = spp_tc_parser_feed(&parser, bytes[ARRAY_SIZE(bytes)-1]);

	TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_DONE, result,
				  "last octet");

	const uint64_t dtn_timestamp = spp_tc_get_dtn_timestamp(&parser);

	TEST_ASSERT_EQUAL_UINT64(23, dtn_timestamp);
}

TEST(spp_timecodes, parse_unsegmented_with_useless_second_p)
{
	struct spp_tc_context_t ctx;

	ctx.with_p_field = true;

	struct spp_tc_parser_t parser;

	spp_tc_parser_init(&ctx, &parser);

	static const uint8_t bytes[] = {
		0x9e,  /* preamble with useless second byte */
		0x00,
		0x4e, 0xff, 0xa2, 0x17, 0x23, 0x42,
	};

	{
		int result = spp_tc_parser_feed(&parser, bytes[0]);

		TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_GOOD, result,
					  "p field 1");
	}

	{
		int result = spp_tc_parser_feed(&parser, bytes[1]);

		TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_GOOD, result,
					  "p field 2");
	}

	for (unsigned int i = 2; i < ARRAY_SIZE(bytes)-1; ++i) {
		int result = spp_tc_parser_feed(&parser, bytes[i]);

		TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_GOOD, result,
					  "tc data");
	}

	int result = spp_tc_parser_feed(&parser, bytes[ARRAY_SIZE(bytes)-1]);

	TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_DONE, result,
				  "last octet");

	const uint64_t dtn_timestamp = spp_tc_get_dtn_timestamp(&parser);

	TEST_ASSERT_EQUAL_UINT64(23, dtn_timestamp);
}

TEST(spp_timecodes, parse_unsegmented_with_second_p)
{
	struct spp_tc_context_t ctx;

	ctx.with_p_field = true;

	struct spp_tc_parser_t parser;

	spp_tc_parser_init(&ctx, &parser);

	static const uint8_t bytes[] = {
		0x9e,  /* preamble */
		0x28,
		0x00, 0x4e, 0xff, 0xa2, 0x17,
		0x23, 0x42, 0x13, 0x37,
	};

	{
		int result = spp_tc_parser_feed(&parser, bytes[0]);

		TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_GOOD, result,
					  "p field 1");
	}

	{
		int result = spp_tc_parser_feed(&parser, bytes[1]);

		TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_GOOD, result,
					  "p field 2");
	}

	for (unsigned int i = 2; i < ARRAY_SIZE(bytes)-1; ++i) {
		int result = spp_tc_parser_feed(&parser, bytes[i]);

		TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_GOOD, result,
					  "tc data");
	}

	int result = spp_tc_parser_feed(&parser, bytes[ARRAY_SIZE(bytes)-1]);

	TEST_ASSERT_EQUAL_MESSAGE(SPP_TC_PARSER_DONE, result,
				  "last octet");

	const uint64_t dtn_timestamp = spp_tc_get_dtn_timestamp(&parser);

	TEST_ASSERT_EQUAL_UINT64(23, dtn_timestamp);
}

TEST(spp_timecodes, configure_from_preamble)
{
	struct spp_tc_context_t ctx;

	static const uint8_t bytes[] = {
		0xae,
		0x28,
	};

	spp_tc_configure_from_preamble(&ctx, &bytes[0], 2);

	TEST_ASSERT_EQUAL(SPP_TC_UNSEGMENTED_CUSTOM_EPOCH, ctx.defaults.type);
	TEST_ASSERT_EQUAL(5, ctx.defaults.unsegmented.base_unit_octets);
	TEST_ASSERT_EQUAL(4, ctx.defaults.unsegmented.fractional_octets);
}

TEST(spp_timecodes, serialize_unsegmented_with_p_narrow)
{
	const uint64_t ts = 577279245;
	const uint32_t ctr = 0x000676ab;

	struct spp_tc_context_t ctx;

	ctx.with_p_field = true;
	ctx.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	ctx.defaults.unsegmented.base_unit_octets = 3;
	ctx.defaults.unsegmented.fractional_octets = 2;

	static const uint8_t reference_bytes[] = {
		0x1a,  /* preamble */
		0x68, 0x37, 0x0d, 0x00, 0x06,
	};

	uint8_t buffer[1024];
	uint8_t *dest = &buffer[0];

	int result = spp_tc_serialize(&ctx, ts, ctr, &dest);

	TEST_ASSERT_EQUAL(0, result);

	const size_t nwritten = dest - &buffer[0];

	TEST_ASSERT_EQUAL(ARRAY_SIZE(reference_bytes), nwritten);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(&reference_bytes[0], &buffer[0],
			nwritten);
}

TEST(spp_timecodes, serialize_unsegmented_with_p_wide)
{
	const uint64_t ts = 577279245;
	const uint32_t ctr = 0x000676ab;

	struct spp_tc_context_t ctx;

	ctx.with_p_field = true;
	ctx.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	ctx.defaults.unsegmented.base_unit_octets = 4;
	ctx.defaults.unsegmented.fractional_octets = 4;

	static const uint8_t reference_bytes[] = {
		0x9f,  /* preamble */
		0x04,
		0x71, 0x68, 0x37, 0x0d, 0x00, 0x06, 0x76, 0xab,
	};

	uint8_t buffer[1024];
	uint8_t *dest = &buffer[0];

	int result = spp_tc_serialize(&ctx, ts, ctr, &dest);

	TEST_ASSERT_EQUAL(0, result);

	const size_t nwritten = dest - &buffer[0];

	TEST_ASSERT_EQUAL(ARRAY_SIZE(reference_bytes), nwritten);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(&reference_bytes[0], &buffer[0],
			nwritten);
}

TEST(spp_timecodes, serialize_unsegmented_without_p_narrow)
{
	const uint64_t ts = 577279245;
	const uint32_t ctr = 0x000676ab;

	struct spp_tc_context_t ctx;

	ctx.with_p_field = false;
	ctx.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	ctx.defaults.unsegmented.base_unit_octets = 3;
	ctx.defaults.unsegmented.fractional_octets = 2;

	static const uint8_t reference_bytes[] = {
		0x68, 0x37, 0x0d, 0x00, 0x06,
	};

	uint8_t buffer[1024];
	uint8_t *dest = &buffer[0];

	int result = spp_tc_serialize(&ctx, ts, ctr, &dest);

	TEST_ASSERT_EQUAL(0, result);

	const size_t nwritten = dest - &buffer[0];

	TEST_ASSERT_EQUAL(ARRAY_SIZE(reference_bytes), nwritten);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(&reference_bytes[0], &buffer[0],
			nwritten);
}

TEST(spp_timecodes, serialize_unsegmented_without_p_wide)
{
	const uint64_t ts = 577279245;
	const uint32_t ctr = 0x000676ab;

	struct spp_tc_context_t ctx;

	ctx.with_p_field = false;
	ctx.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	ctx.defaults.unsegmented.base_unit_octets = 4;
	ctx.defaults.unsegmented.fractional_octets = 4;

	static const uint8_t reference_bytes[] = {
		0x71, 0x68, 0x37, 0x0d, 0x00, 0x06, 0x76, 0xab,
	};

	uint8_t buffer[1024];
	uint8_t *dest = &buffer[0];

	int result = spp_tc_serialize(&ctx, ts, ctr, &dest);

	TEST_ASSERT_EQUAL(0, result);

	const size_t nwritten = dest - &buffer[0];

	TEST_ASSERT_EQUAL(ARRAY_SIZE(reference_bytes), nwritten);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(&reference_bytes[0], &buffer[0],
			nwritten);
}

TEST(spp_timecodes, serialize_unsegmented_without_p_nofrac)
{
	const uint64_t ts = 577279245;
	const uint32_t ctr = 0x000676ab;

	struct spp_tc_context_t ctx;

	ctx.with_p_field = false;
	ctx.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	ctx.defaults.unsegmented.base_unit_octets = 4;
	ctx.defaults.unsegmented.fractional_octets = 0;

	static const uint8_t reference_bytes[] = {
		0x71, 0x68, 0x37, 0x0d
	};

	uint8_t buffer[1024];
	uint8_t *dest = &buffer[0];

	int result = spp_tc_serialize(&ctx, ts, ctr, &dest);

	TEST_ASSERT_EQUAL(0, result);

	const size_t nwritten = dest - &buffer[0];

	TEST_ASSERT_EQUAL(ARRAY_SIZE(reference_bytes), nwritten);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(&reference_bytes[0], &buffer[0],
			nwritten);
}

TEST(spp_timecodes, get_size_unsegmented_with_p_nofrac)
{
	struct spp_tc_context_t ctx;

	ctx.with_p_field = true;
	ctx.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	ctx.defaults.unsegmented.base_unit_octets = 4;
	ctx.defaults.unsegmented.fractional_octets = 0;

	TEST_ASSERT_EQUAL(5, spp_tc_get_size(&ctx));
}

TEST(spp_timecodes, get_size_unsegmented_without_p_nofrac)
{
	struct spp_tc_context_t ctx;

	ctx.with_p_field = false;
	ctx.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	ctx.defaults.unsegmented.base_unit_octets = 4;
	ctx.defaults.unsegmented.fractional_octets = 0;

	TEST_ASSERT_EQUAL(4, spp_tc_get_size(&ctx));
}

TEST(spp_timecodes, get_size_unsegmented_with_long_p)
{
	struct spp_tc_context_t ctx;

	ctx.with_p_field = true;
	ctx.defaults.type = SPP_TC_UNSEGMENTED_CCSDS_EPOCH;
	ctx.defaults.unsegmented.base_unit_octets = 5;
	ctx.defaults.unsegmented.fractional_octets = 1;

	TEST_ASSERT_EQUAL(8, spp_tc_get_size(&ctx));
}

TEST_GROUP_RUNNER(spp_timecodes)
{
	RUN_TEST_CASE(spp_timecodes,
		      parse_unsegmented_without_p_no_fractional);
	RUN_TEST_CASE(spp_timecodes,
		      parse_unsegmented_without_p_with_fractional);
	RUN_TEST_CASE(spp_timecodes,
		      parse_unsegmented_with_p_fractional);
	RUN_TEST_CASE(spp_timecodes,
		      parse_unsegmented_with_useless_second_p);
	RUN_TEST_CASE(spp_timecodes,
		      parse_unsegmented_with_second_p);
	RUN_TEST_CASE(spp_timecodes,
		      configure_from_preamble);
	RUN_TEST_CASE(spp_timecodes,
		      serialize_unsegmented_with_p_narrow);
	RUN_TEST_CASE(spp_timecodes,
		      serialize_unsegmented_with_p_wide);
	RUN_TEST_CASE(spp_timecodes,
		      serialize_unsegmented_without_p_narrow);
	RUN_TEST_CASE(spp_timecodes,
		      serialize_unsegmented_without_p_wide);
	RUN_TEST_CASE(spp_timecodes,
		      serialize_unsegmented_without_p_nofrac);
	RUN_TEST_CASE(spp_timecodes,
		      get_size_unsegmented_with_p_nofrac);
	RUN_TEST_CASE(spp_timecodes,
		      get_size_unsegmented_without_p_nofrac);
	RUN_TEST_CASE(spp_timecodes,
		      get_size_unsegmented_with_long_p);
}
