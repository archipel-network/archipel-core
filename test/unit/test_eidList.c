#include "ud3tn/eid_list.h"

#include "unity_fixture.h"

#include <stddef.h>
#include <stdlib.h>

TEST_GROUP(eidList);

static const uint8_t input[] = {
	3,
	0x41, 0x42, 0x43, 0x00,
	0x61, 0x62, 0x63, 0x64, 0x00,
	0x0A, 0x00
};

static const char *const output[] = {
	"ABC",
	"abcd",
	"\n"
};

TEST_SETUP(eidList)
{
}

TEST_TEAR_DOWN(eidList)
{
}

TEST(eidList, eidList_decode)
{
	size_t i;
	char **result = eidlist_decode(input, sizeof(input));

	TEST_ASSERT_NOT_NULL(result);
	for (i = 0; i < sizeof(output) / sizeof(char *); i++) {
		TEST_ASSERT_EQUAL_STRING(output[i], result[i]);
		free(result[i]);
	}
	free(result);
}

TEST(eidList, eidList_encode)
{
	int len;
	uint8_t *result = eidlist_encode(
		output, sizeof(output) / sizeof(char *), &len);

	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_EQUAL_INT(sizeof(input), len);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(input, result, len);
	free(result);
}

TEST_GROUP_RUNNER(eidList)
{
	RUN_TEST_CASE(eidList, eidList_decode);
	RUN_TEST_CASE(eidList, eidList_encode);
}
