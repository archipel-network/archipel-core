#ifdef PLATFORM_POSIX

#include "platform/posix/simple_queue.h"

#include "unity_fixture.h"

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

TEST_GROUP(simple_queue);

TEST_SETUP(simple_queue)
{
}

TEST_TEAR_DOWN(simple_queue)
{
}

TEST(simple_queue, test_createQueue)
{
	// create a queue
	Queue_t *q = queueCreate(2, 4);

	TEST_ASSERT_NOT_NULL(q);
	TEST_ASSERT_EQUAL_UINT(q->item_length, 2);
	TEST_ASSERT_EQUAL_UINT(q->item_size, 4);
	TEST_ASSERT_NOT_NULL(q->abs_start);
	TEST_ASSERT_NOT_NULL(q->abs_end);
	TEST_ASSERT_NOT_NULL(q->current_start);
	TEST_ASSERT_NOT_NULL(q->current_end);

	TEST_ASSERT_EQUAL_PTR(q->abs_start, q->current_start);
	TEST_ASSERT_EQUAL_PTR(q->abs_start, q->current_end);

	TEST_ASSERT_EQUAL_PTR(q->abs_end, q->abs_start+8);
}

TEST(simple_queue, test_PushPopBasic)
{
	// create a queue
	Queue_t *q = queueCreate(1, sizeof(int));

	const int i = 42;

	TEST_ASSERT_EQUAL_PTR(q->abs_start, q->current_start);
	TEST_ASSERT_EQUAL_PTR(q->abs_start, q->current_end);

	TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, false));

	TEST_ASSERT_EQUAL_INT(i, (int)(q->current_start)[0]);

	int j;

	TEST_ASSERT_EQUAL_INT(0, queuePop(q, &j, 0));

	TEST_ASSERT_EQUAL_INT(i, j);
}

TEST(simple_queue, test_PushFullPopFull)
{
	// create a queue
	Queue_t *q = queueCreate(10, sizeof(int));

	int i, j;

	for (i = 0; i <= 9; i++) {
		// the first ten insertions should be successful
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, false));
	}
	// the eleventh insertion (without force) should fail
	TEST_ASSERT_EQUAL_INT(1, queuePush(q, &i, 0, false));

	for (i = 0; i <= 9; i++) {
		// the first ten removals should be successful
		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &j, 0));
		// the removed values should have the correct order
		TEST_ASSERT_EQUAL_INT(i, j);
	}

	// the eleventh removal should fail
	TEST_ASSERT_EQUAL_INT(1, queuePop(q, &j, 0));
}

TEST(simple_queue, test_PushEndPopEnd)
{
	// create a queue
	Queue_t *q = queueCreate(10, sizeof(int));

	int i, j;

	for (i = 0; i <= 9; i++) {
		// the first ten insertions should be successful
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, false));
	}

	for (i = 0; i <= 9; i++) {
		// the first ten removals should be successful
		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &j, 0));
		// the removed values should have the correct order
		TEST_ASSERT_EQUAL_INT(i, j);
	}

	// from here on we should start at the abs_start of the queue again!

	for (i = 20; i <= 29; i++) {
		// the first ten insertions should be successful
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, false));
	}

	for (i = 20; i <= 29; i++) {
		// the first ten removals should be successful
		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &j, 0));
		// the removed values should have the correct order
		TEST_ASSERT_EQUAL_INT(i, j);
	}

}

TEST(simple_queue, test_CircularBehaviour)
{
	// create a queue
	Queue_t *q = queueCreate(10, sizeof(int));

	int i, j, k, l, m, n;

	// test for two circular rounds with two elements
	for (i = 0; i <= 19; i += 2) {
		j = i+1;
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, false));
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &j, 0, false));

		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &k, 0));
		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &l, 0));
		// the removed values should have the correct values
		TEST_ASSERT_EQUAL_INT(i, k);
		TEST_ASSERT_EQUAL_INT(j, l);
	}

	// test for three circular rounds with three elements
	for (i = 0; i <= 29; i += 3) {
		j = i + 1;
		k = j + 1;
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, false));
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &j, 0, false));
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &k, 0, false));

		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &l, 0));
		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &m, 0));
		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &n, 0));
		// the removed values should have the correct values
		TEST_ASSERT_EQUAL_INT(i, l);
		TEST_ASSERT_EQUAL_INT(j, m);
		TEST_ASSERT_EQUAL_INT(k, n);
	}
}

TEST(simple_queue, test_ResetQueue)
{
	// create a queue
	Queue_t *q = queueCreate(10, sizeof(int));

	int i, j;

	for (i = 0; i <= 5; i++)
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, false));

	for (i = 0; i <= 3; i++) {
		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &j, 0));
		TEST_ASSERT_EQUAL_INT(i, j);
	}

	// check that the ptrs are not equal
	TEST_ASSERT_NOT_EQUAL(q->abs_start, q->current_start);
	TEST_ASSERT_NOT_EQUAL(q->abs_start, q->current_end);

	int value_pop, value_push;

	sem_getvalue(&q->sem_pop, &value_pop);
	sem_getvalue(&q->sem_push, &value_push);

	TEST_ASSERT_EQUAL_INT(2, value_pop);
	TEST_ASSERT_EQUAL_INT(8, value_push);

	queueReset(q);

	TEST_ASSERT_EQUAL_PTR(q->abs_start, q->current_start);
	TEST_ASSERT_EQUAL_PTR(q->abs_start, q->current_end);

	sem_getvalue(&q->sem_pop, &value_pop);
	sem_getvalue(&q->sem_push, &value_push);

	TEST_ASSERT_EQUAL_INT(0, value_pop);
	TEST_ASSERT_EQUAL_INT(10, value_push);

}

TEST(simple_queue, test_NrOfWaitingElements)
{
	// create a queue
	Queue_t *q = queueCreate(10, sizeof(int));

	int i, j;

	for (i = 0; i <= 8; i++)
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, false));

	TEST_ASSERT_EQUAL_UINT(9, queueItemsWaiting(q));

	for (i = 0; i <= 3; i++) {
		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &j, 0));
		TEST_ASSERT_EQUAL_INT(i, j);
	}

	TEST_ASSERT_EQUAL_UINT(5, queueItemsWaiting(q));

	for (i = 4; i <= 8; i++) {
		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &j, 0));
		TEST_ASSERT_EQUAL_INT(i, j);
	}

	TEST_ASSERT_EQUAL_UINT(0, queueItemsWaiting(q));

}

TEST(simple_queue, test_ForcePush)
{
	// create a queue
	Queue_t *q = queueCreate(10, sizeof(int));

	int i, j;

	for (i = 0; i <= 9; i++) {
		// the first ten insertions should be successful
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, false));
	}
	// the eleventh insertion (without force) should fail
	TEST_ASSERT_EQUAL_INT(1, queuePush(q, &i, 0, false));

	i = 42;

	// the eleventh insertion (with force) should succeed
	TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, true));

	for (i = 0; i <= 8; i++) {
		// the first nine removals should be standard
		TEST_ASSERT_EQUAL_INT(0, queuePop(q, &j, 0));
		// the removed values should have the correct order
		TEST_ASSERT_EQUAL_INT(i, j);
	}

	// the last removal should still be successful
	TEST_ASSERT_EQUAL_INT(0, queuePop(q, &j, 0));
	// the removed value should have the forced value
	TEST_ASSERT_EQUAL_INT(42, j);
}

// returns difference in ms
int ms_diff(struct timespec *start, struct timespec *stop)
{
	struct timespec result;

	if ((stop->tv_nsec - start->tv_nsec) < 0) {
		result.tv_sec = stop->tv_sec - start->tv_sec - 1;
		result.tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result.tv_sec = stop->tv_sec - start->tv_sec;
		result.tv_nsec = stop->tv_nsec - start->tv_nsec;
	}

	return result.tv_sec * 1000 + (result.tv_nsec / 1000000);
}

TEST(simple_queue, test_SemaphoreTimingBehaviour)
{
	struct timespec ts1, ts2;

	// create a queue
	Queue_t *q = queueCreate(10, sizeof(int));

	int i, j;

	// a removal with timeout 0 should fail immediately
	clock_gettime(CLOCK_REALTIME, &ts1);
	TEST_ASSERT_EQUAL_INT(1, queuePop(q, &j, 0));
	clock_gettime(CLOCK_REALTIME, &ts2);
	// allow a little deviation due to the overhead
	TEST_ASSERT_TRUE(ms_diff(&ts1, &ts2) < 2);

	// a removal with timeout 100ms should fail eventually
	clock_gettime(CLOCK_REALTIME, &ts1);
	TEST_ASSERT_EQUAL_INT(1, queuePop(q, &j, 100));
	clock_gettime(CLOCK_REALTIME, &ts2);
	// allow a little deviation due to the overhead
	TEST_ASSERT_TRUE(ms_diff(&ts1, &ts2) > 98);
	TEST_ASSERT_TRUE(ms_diff(&ts1, &ts2) < 102);

	// a removal with timeout 2000ms should fail eventually
	clock_gettime(CLOCK_REALTIME, &ts1);
	TEST_ASSERT_EQUAL_INT(1, queuePop(q, &j, 2000));
	clock_gettime(CLOCK_REALTIME, &ts2);
	// allow a little deviation due to the overhead
	TEST_ASSERT_TRUE(ms_diff(&ts1, &ts2) > 1998);
	TEST_ASSERT_TRUE(ms_diff(&ts1, &ts2) < 2002);


	for (i = 0; i <= 9; i++) {
		// the first ten insertions should be successful
		TEST_ASSERT_EQUAL_INT(0, queuePush(q, &i, 0, false));
	}

	// a insertion with timeout 0 should fail immediately
	clock_gettime(CLOCK_REALTIME, &ts1);
	TEST_ASSERT_EQUAL_INT(1, queuePush(q, &i, 0, false));
	clock_gettime(CLOCK_REALTIME, &ts2);
	// allow a little deviation due to the overhead
	TEST_ASSERT_TRUE(ms_diff(&ts1, &ts2) < 2);

	// a insertion with timeout 100ms should fail eventually
	clock_gettime(CLOCK_REALTIME, &ts1);
	TEST_ASSERT_EQUAL_INT(1, queuePush(q, &i, 100, false));
	clock_gettime(CLOCK_REALTIME, &ts2);
	// allow a little deviation due to the overhead
	TEST_ASSERT_TRUE(ms_diff(&ts1, &ts2) > 98);
	TEST_ASSERT_TRUE(ms_diff(&ts1, &ts2) < 102);

	// a insertion with timeout 2000ms should fail eventually
	clock_gettime(CLOCK_REALTIME, &ts1);
	TEST_ASSERT_EQUAL_INT(1, queuePush(q, &i, 2000, false));
	clock_gettime(CLOCK_REALTIME, &ts2);
	// allow a little deviation due to the overhead
	TEST_ASSERT_TRUE(ms_diff(&ts1, &ts2) > 1998);
	TEST_ASSERT_TRUE(ms_diff(&ts1, &ts2) < 2002);
}

TEST_GROUP_RUNNER(simple_queue)
{
	RUN_TEST_CASE(simple_queue, test_createQueue);
	RUN_TEST_CASE(simple_queue, test_PushPopBasic);
	RUN_TEST_CASE(simple_queue, test_PushFullPopFull);
	RUN_TEST_CASE(simple_queue, test_PushEndPopEnd);
	RUN_TEST_CASE(simple_queue, test_CircularBehaviour);
	RUN_TEST_CASE(simple_queue, test_ResetQueue);
	RUN_TEST_CASE(simple_queue, test_NrOfWaitingElements);
	RUN_TEST_CASE(simple_queue, test_ForcePush);
	RUN_TEST_CASE(simple_queue, test_SemaphoreTimingBehaviour);
}

#endif // PLATFORM_POSIX
