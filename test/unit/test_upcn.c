#include "platform/hal_time.h"

#include "unity_fixture.h"

TEST_GROUP(upcn);

TEST_SETUP(upcn)
{
}

TEST_TEAR_DOWN(upcn)
{
}

TEST(upcn, hal_time)
{
	hal_time_init(1234);
	TEST_ASSERT_EQUAL_UINT64(1234, hal_time_get_timestamp_s());
	hal_time_init(0);
	TEST_ASSERT_EQUAL_UINT64(0, hal_time_get_timestamp_s());
}

TEST_GROUP_RUNNER(upcn)
{
	RUN_TEST_CASE(upcn, hal_time);
}
