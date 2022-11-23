// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "platform/hal_time.h"

#include "unity_fixture.h"

#include <stdint.h>
#include <time.h>

TEST_GROUP(ud3tn);

TEST_SETUP(ud3tn)
{
}

TEST_TEAR_DOWN(ud3tn)
{
}

TEST(ud3tn, hal_time)
{
	hal_time_init(1234);
	TEST_ASSERT_EQUAL_UINT64(1234, hal_time_get_timestamp_s());
	hal_time_init(0);
	TEST_ASSERT_EQUAL_UINT64(0, hal_time_get_timestamp_s());

	struct timespec ts;

	// use system time
	hal_time_init(UINT64_MAX);
	clock_gettime(CLOCK_REALTIME, &ts);

	TEST_ASSERT_EQUAL_UINT64(ts.tv_sec - DTN_TIMESTAMP_OFFSET,
				 hal_time_get_timestamp_s());
	TEST_ASSERT_EQUAL_UINT64(ts.tv_sec - DTN_TIMESTAMP_OFFSET,
				 hal_time_get_timestamp_ms() / 1000);
}

TEST_GROUP_RUNNER(ud3tn)
{
	RUN_TEST_CASE(ud3tn, hal_time);
}
