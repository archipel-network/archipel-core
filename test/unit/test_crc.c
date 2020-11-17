#include "ud3tn/crc.h"

#include "unity_fixture.h"

#include <stdio.h>


const uint8_t m1[] = "";
const uint8_t m2[] = "asdf";
const uint8_t m3[] = "message";
const uint8_t m4[] = "Some really long message";


TEST_GROUP(crc);

TEST_SETUP(crc)
{
}

TEST_TEAR_DOWN(crc)
{
}

TEST(crc, crc16_x25)
{
	struct crc_stream crc;

	TEST_ASSERT_EQUAL_HEX16(0x0000, crc16_x25(m1, sizeof(m1) - 1));
	TEST_ASSERT_EQUAL_HEX16(0x1238, crc16_x25(m2, sizeof(m2) - 1));
	TEST_ASSERT_EQUAL_HEX16(0xa15c, crc16_x25(m3, sizeof(m3) - 1));
	TEST_ASSERT_EQUAL_HEX16(0xffce, crc16_x25(m4, sizeof(m4) - 1));

	// Test CRC streamed calculation
	crc_init(&crc, CRC16_X25);

	// read data stream
	for (int i = 0; m4[i] != 0; i++)
		crc.feed(&crc, m4[i]);

	// finish CRC calculation
	crc.feed_eof(&crc);

	TEST_ASSERT_EQUAL_HEX16(0xffce, crc.checksum);
}

TEST(crc, crc16_ccitt_false)
{
	struct crc_stream crc;

	TEST_ASSERT_EQUAL_HEX16(0xffff, crc16_ccitt_false(m1, sizeof(m1) - 1));
	TEST_ASSERT_EQUAL_HEX16(0xe170, crc16_ccitt_false(m2, sizeof(m2) - 1));
	TEST_ASSERT_EQUAL_HEX16(0x9cdf, crc16_ccitt_false(m3, sizeof(m3) - 1));
	TEST_ASSERT_EQUAL_HEX16(0x3b8b, crc16_ccitt_false(m4, sizeof(m4) - 1));

	// Test CRC streamed calculation
	crc_init(&crc, CRC16_CCITT_FALSE);

	// read data stream
	for (int i = 0; m4[i] != 0; i++)
		crc.feed(&crc, m4[i]);

	// finish CRC calculation
	crc.feed_eof(&crc);

	TEST_ASSERT_EQUAL_HEX16(0x3b8b, crc.checksum);
}

TEST(crc, crc32)
{
	struct crc_stream crc;

	TEST_ASSERT_EQUAL_HEX32(0x00000000, crc32(m1, sizeof(m1) - 1));
	TEST_ASSERT_EQUAL_HEX32(0xdc7d5c77, crc32(m2, sizeof(m2) - 1));
	TEST_ASSERT_EQUAL_HEX32(0x98a214d0, crc32(m3, sizeof(m3) - 1));
	TEST_ASSERT_EQUAL_HEX32(0xee7f4af1, crc32(m4, sizeof(m4) - 1));

	// Test CRC streamed calculation
	crc_init(&crc, CRC32);

	// read data stream
	for (int i = 0; m4[i] != 0; i++)
		crc.feed(&crc, m4[i]);

	// finish CRC calculation
	crc.feed_eof(&crc);

	TEST_ASSERT_EQUAL_HEX32(0xee7f4af1, crc.checksum);
}

TEST_GROUP_RUNNER(crc)
{
	RUN_TEST_CASE(crc, crc16_x25);
	RUN_TEST_CASE(crc, crc16_ccitt_false);
	RUN_TEST_CASE(crc, crc32);
}
