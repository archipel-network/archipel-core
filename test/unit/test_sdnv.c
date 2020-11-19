#include "bundle6/sdnv.h"

#include "ud3tn/common.h"

#include "unity_fixture.h"

TEST_GROUP(sdnv);

TEST_SETUP(sdnv)
{
}

TEST_TEAR_DOWN(sdnv)
{
}

TEST(sdnv, sdnv_get_size)
{
	/* u16 */
	TEST_ASSERT_EQUAL_INT(1, sdnv_get_size_u16(0));
	TEST_ASSERT_EQUAL_INT(1, sdnv_get_size_u16(0x7F));
	TEST_ASSERT_EQUAL_INT(2, sdnv_get_size_u16(0x80));
	TEST_ASSERT_EQUAL_INT(2, sdnv_get_size_u16(0x2000));
	TEST_ASSERT_EQUAL_INT(3, sdnv_get_size_u16(0x4000));
	TEST_ASSERT_EQUAL_INT(3, sdnv_get_size_u16(0xFFFF));
	/* u32 */
	TEST_ASSERT_EQUAL_INT(1, sdnv_get_size_u32(0));
	TEST_ASSERT_EQUAL_INT(1, sdnv_get_size_u32(0x7F));
	TEST_ASSERT_EQUAL_INT(2, sdnv_get_size_u32(0x80));
	TEST_ASSERT_EQUAL_INT(2, sdnv_get_size_u32(0x3FFF));
	TEST_ASSERT_EQUAL_INT(3, sdnv_get_size_u32(0x4000));
	TEST_ASSERT_EQUAL_INT(3, sdnv_get_size_u32(0x100000));
	TEST_ASSERT_EQUAL_INT(4, sdnv_get_size_u32(0x200000));
	TEST_ASSERT_EQUAL_INT(4, sdnv_get_size_u32(0xFFFFFFF));
	TEST_ASSERT_EQUAL_INT(5, sdnv_get_size_u32(0x10000000));
	TEST_ASSERT_EQUAL_INT(5, sdnv_get_size_u32(0xFFFFFFFF));
	/* u64 */
	TEST_ASSERT_EQUAL_INT(1, sdnv_get_size_u64(0));
	TEST_ASSERT_EQUAL_INT(1, sdnv_get_size_u64(0x7F));
	TEST_ASSERT_EQUAL_INT(2, sdnv_get_size_u64(0x80));
	TEST_ASSERT_EQUAL_INT(2, sdnv_get_size_u64(0x3FFF));
	TEST_ASSERT_EQUAL_INT(3, sdnv_get_size_u64(0x4000));
	TEST_ASSERT_EQUAL_INT(3, sdnv_get_size_u64(0x1FFFFF));
	TEST_ASSERT_EQUAL_INT(4, sdnv_get_size_u64(0x200000));
	TEST_ASSERT_EQUAL_INT(4, sdnv_get_size_u64(0xFFFFFFF));
	TEST_ASSERT_EQUAL_INT(5, sdnv_get_size_u64(0x10000000));
	TEST_ASSERT_EQUAL_INT(5, sdnv_get_size_u64(0x7FFFFFFFF));
	TEST_ASSERT_EQUAL_INT(6, sdnv_get_size_u64(0x800000000));
	TEST_ASSERT_EQUAL_INT(6, sdnv_get_size_u64(0x3FFFFFFFFFF));
	TEST_ASSERT_EQUAL_INT(7, sdnv_get_size_u64(0x40000000000));
	TEST_ASSERT_EQUAL_INT(7, sdnv_get_size_u64(0x1FFFFFFFFFFFF));
	TEST_ASSERT_EQUAL_INT(8, sdnv_get_size_u64(0x2000000000000));
	TEST_ASSERT_EQUAL_INT(8, sdnv_get_size_u64(0xFFFFFFFFFFFFFF));
	TEST_ASSERT_EQUAL_INT(9, sdnv_get_size_u64(0x100000000000000));
	TEST_ASSERT_EQUAL_INT(9, sdnv_get_size_u64(0x7FFFFFFFFFFFFFFF));
	TEST_ASSERT_EQUAL_INT(10, sdnv_get_size_u64(0x8000000000000000));
	TEST_ASSERT_EQUAL_INT(10, sdnv_get_size_u64(0xFFFFFFFFFFFFFFFF));
}

static const uint16_t VAL_MAX16 = 0xFFFF;
static const uint32_t VAL_MAX32 = 0xFFFFFFFF;
static const uint64_t VAL_MAX64 = 0xFFFFFFFFFFFFFFFF;
static const uint8_t ARR_MAX16[] = { 0x83, 0xFF, 0x7F };
static const uint8_t ARR_MAX32[] = { 0x8F, 0xFF, 0xFF, 0xFF, 0x7F };
static const uint8_t ARR_MAX64[] = { 0x81, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0x7F };
static const uint8_t ARR_ERR16[] = { 0x87, 0xFF, 0x7F };
static const uint8_t ARR_ERR32[] = { 0x9F, 0xFF, 0xFF, 0xFF, 0x7F };
static const uint8_t ARR_ERR64[] = { 0x83, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0x7F };

static const uint16_t VAL_TST16 = 0x4D03;
static const uint8_t ARR_TST16[] = { 0x81, 0x9A, 0x03 };

TEST(sdnv, sdnv_write)
{
	uint8_t buffer[10];

	TEST_ASSERT_EQUAL_INT(1, sdnv_write_u16(buffer, 0));
	TEST_ASSERT_EQUAL_HEX8(0, buffer[0]);
	TEST_ASSERT_EQUAL_INT(1, sdnv_write_u32(buffer, 0));
	TEST_ASSERT_EQUAL_HEX8(0, buffer[0]);
	TEST_ASSERT_EQUAL_INT(1, sdnv_write_u64(buffer, 0));
	TEST_ASSERT_EQUAL_HEX8(0, buffer[0]);
	TEST_ASSERT_EQUAL_INT(3, sdnv_write_u16(buffer, VAL_MAX16));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(ARR_MAX16, buffer, 3);
	TEST_ASSERT_EQUAL_INT(5, sdnv_write_u32(buffer, VAL_MAX32));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(ARR_MAX32, buffer, 5);
	TEST_ASSERT_EQUAL_INT(10, sdnv_write_u64(buffer, VAL_MAX64));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(ARR_MAX64, buffer, 10);
	TEST_ASSERT_EQUAL_INT(3, sdnv_write_u16(buffer, VAL_TST16));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(ARR_TST16, buffer, 3);
	TEST_ASSERT_EQUAL_INT(3, sdnv_write_u32(buffer, VAL_TST16));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(ARR_TST16, buffer, 3);
	TEST_ASSERT_EQUAL_INT(3, sdnv_write_u64(buffer, VAL_TST16));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(ARR_TST16, buffer, 3);
}

TEST(sdnv, sdnv_read)
{
	struct sdnv_state s;
	size_t i;
	uint16_t t16;
	uint32_t t32;
	uint64_t t64;

	/* max16 */
	i = 0;
	t16 = 0;
	sdnv_reset(&s);
	while (s.status == SDNV_IN_PROGRESS && i < ARRAY_LENGTH(ARR_MAX16))
		sdnv_read_u16(&s, &t16, ARR_MAX16[i++]);
	TEST_ASSERT_EQUAL_INT(SDNV_DONE, s.status);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR_NONE, s.error);
	TEST_ASSERT_EQUAL_HEX16(VAL_MAX16, t16);
	TEST_ASSERT_EQUAL_INT(3, i);
	/* tst16 */
	i = 0;
	t16 = 0;
	sdnv_reset(&s);
	while (s.status == SDNV_IN_PROGRESS && i < ARRAY_LENGTH(ARR_TST16))
		sdnv_read_u16(&s, &t16, ARR_TST16[i++]);
	TEST_ASSERT_EQUAL_INT(SDNV_DONE, s.status);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR_NONE, s.error);
	TEST_ASSERT_EQUAL_HEX16(VAL_TST16, t16);
	TEST_ASSERT_EQUAL_INT(3, i);
	/* err16 */
	i = 0;
	t16 = 0;
	sdnv_reset(&s);
	while (s.status == SDNV_IN_PROGRESS && i < ARRAY_LENGTH(ARR_ERR16))
		sdnv_read_u16(&s, &t16, ARR_ERR16[i++]);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR, s.status);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR_OVERFLOW, s.error);
	TEST_ASSERT_EQUAL_INT(3, i);
	/* max32 */
	i = 0;
	t32 = 0;
	sdnv_reset(&s);
	while (s.status == SDNV_IN_PROGRESS && i < ARRAY_LENGTH(ARR_MAX32))
		sdnv_read_u32(&s, &t32, ARR_MAX32[i++]);
	TEST_ASSERT_EQUAL_INT(SDNV_DONE, s.status);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR_NONE, s.error);
	TEST_ASSERT_EQUAL_HEX32(VAL_MAX32, t32);
	TEST_ASSERT_EQUAL_INT(5, i);
	/* tst32 */
	i = 0;
	t32 = 0;
	sdnv_reset(&s);
	while (s.status == SDNV_IN_PROGRESS && i < ARRAY_LENGTH(ARR_TST16))
		sdnv_read_u32(&s, &t32, ARR_TST16[i++]);
	TEST_ASSERT_EQUAL_INT(SDNV_DONE, s.status);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR_NONE, s.error);
	TEST_ASSERT_EQUAL_HEX32(VAL_TST16, t32);
	TEST_ASSERT_EQUAL_INT(3, i);
	/* err32 */
	i = 0;
	t32 = 0;
	sdnv_reset(&s);
	while (s.status == SDNV_IN_PROGRESS && i < ARRAY_LENGTH(ARR_ERR32))
		sdnv_read_u32(&s, &t32, ARR_ERR32[i++]);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR, s.status);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR_OVERFLOW, s.error);
	TEST_ASSERT_EQUAL_INT(5, i);
	/* max64 */
	i = 0;
	t64 = 0;
	sdnv_reset(&s);
	while (s.status == SDNV_IN_PROGRESS && i < ARRAY_LENGTH(ARR_MAX64))
		sdnv_read_u64(&s, &t64, ARR_MAX64[i++]);
	TEST_ASSERT_EQUAL_INT(SDNV_DONE, s.status);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR_NONE, s.error);
	TEST_ASSERT_EQUAL_HEX64(VAL_MAX64, t64);
	TEST_ASSERT_EQUAL_INT(10, i);
	/* tst64 */
	i = 0;
	t64 = 0;
	sdnv_reset(&s);
	while (s.status == SDNV_IN_PROGRESS && i < ARRAY_LENGTH(ARR_TST16))
		sdnv_read_u64(&s, &t64, ARR_TST16[i++]);
	TEST_ASSERT_EQUAL_INT(SDNV_DONE, s.status);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR_NONE, s.error);
	TEST_ASSERT_EQUAL_HEX64(VAL_TST16, t64);
	TEST_ASSERT_EQUAL_INT(3, i);
	/* err64 */
	i = 0;
	t64 = 0;
	sdnv_reset(&s);
	while (s.status == SDNV_IN_PROGRESS && i < ARRAY_LENGTH(ARR_ERR64))
		sdnv_read_u64(&s, &t64, ARR_ERR64[i++]);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR, s.status);
	TEST_ASSERT_EQUAL_INT(SDNV_ERROR_OVERFLOW, s.error);
	TEST_ASSERT_EQUAL_INT(10, i);
}

TEST_GROUP_RUNNER(sdnv)
{
	RUN_TEST_CASE(sdnv, sdnv_get_size);
	RUN_TEST_CASE(sdnv, sdnv_write);
	RUN_TEST_CASE(sdnv, sdnv_read);
}
