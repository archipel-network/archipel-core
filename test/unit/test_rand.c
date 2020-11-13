#include "platform/hal_random.h"

#include "unity_fixture.h"

TEST_GROUP(random);

#define BUCK 10
#define ITER 10000
#define MAX_PERCENT 11

TEST_SETUP(random)
{
}

TEST_TEAR_DOWN(random)
{
}

TEST(random, random_distribution)
{
	uint32_t bucks[BUCK];
	uint32_t i, max;

	for (i = 0; i < BUCK; i++)
		bucks[i] = 0;
	for (i = 0; i < ITER; i++)
		bucks[hal_random_get() % BUCK]++;
	max = MAX_PERCENT * ITER / 100;
	for (i = 0; i < BUCK; i++)
		TEST_ASSERT_TRUE(bucks[i] < max);
}

TEST_GROUP_RUNNER(random)
{
	RUN_TEST_CASE(random, random_distribution);
}
