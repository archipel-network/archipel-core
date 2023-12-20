// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_semaphore.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for semaphore-related functionality
 *
 */

#include "platform/hal_semaphore.h"
#include "platform/posix/hal_types.h"

#include "ud3tn/common.h"
#include "ud3tn/result.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef __APPLE__

// Implementation for Apple platforms, using GCD
// See: https://developer.apple.com/documentation/DISPATCH

#include <dispatch/dispatch.h>

// https://stackoverflow.com/a/27847103
// not exported but used in simple_queue for performance reasons
void hal_semaphore_init_inplace(Semaphore_t sem, int value)
{
	sem->sem = dispatch_semaphore_create(value);
}

void hal_semaphore_take_blocking(Semaphore_t sem)
{
	dispatch_semaphore_wait(sem->sem, DISPATCH_TIME_FOREVER);
}

void hal_semaphore_release(Semaphore_t sem)
{
	dispatch_semaphore_signal(sem->sem);
}

bool hal_semaphore_is_blocked(Semaphore_t sem)
{
	if (dispatch_semaphore_wait(sem->sem, DISPATCH_TIME_NOW) == 0) {
		dispatch_semaphore_signal(sem->sem);
		return 0;
	}
	return 1;
}

void hal_semaphore_delete(Semaphore_t sem)
{
	sem->sem = NULL;
	free(sem);
}

enum ud3tn_result hal_semaphore_try_take(Semaphore_t sem, int64_t timeout_ms)
{
	// Infinite blocking in invalid value range. 9223372036854 is
	// floor(INT64_MAX / 1000000), so we can convert to nanoseconds.
	if (timeout_ms < 0 ||
			(uint64_t)timeout_ms > HAL_SEMAPHORE_MAX_DELAY_MS) {
		hal_semaphore_take_blocking(sem);
		return UD3TN_OK;
	}

	dispatch_time_t dt = dispatch_time(
		DISPATCH_TIME_NOW,
		timeout_ms * 1000000
	);

	return dispatch_semaphore_wait(sem->sem, dt) ? UD3TN_FAIL : UD3TN_OK;
}

#else // __APPLE__

// Implementation for non-Apple (Linux/BSD) platforms

#include <semaphore.h>

// not exported but used in simple_queue for performance reasons
void hal_semaphore_init_inplace(Semaphore_t sem, int value)
{
	sem_init(&sem->sem, 0, value);
}

void hal_semaphore_take_blocking(Semaphore_t sem)
{
	int ret;

	do {
		ret = sem_wait(&sem->sem);
	} while (ret == -1 && errno == EINTR);
}

void hal_semaphore_release(Semaphore_t sem)
{
	sem_post(&sem->sem);
}

bool hal_semaphore_is_blocked(Semaphore_t sem)
{
	int rv = 0;

	sem_getvalue(&sem->sem, &rv);

	return rv == 0;
}

void hal_semaphore_delete(Semaphore_t sem)
{
	sem_destroy(&sem->sem);
	free(sem);
}

enum ud3tn_result hal_semaphore_try_take(Semaphore_t sem, int64_t timeout_ms)
{
	// Infinite blocking in invalid value range. 9223372036854 is
	// floor(INT64_MAX / 1000000), so we can convert to nanoseconds.
	if (timeout_ms < 0 ||
			(uint64_t)timeout_ms > HAL_SEMAPHORE_MAX_DELAY_MS) {
		hal_semaphore_take_blocking(sem);
		return UD3TN_OK;
	}

	int ret;
	struct timespec ts;

	if (timeout_ms == 0)
		return sem_trywait(&sem->sem) == -1 ? UD3TN_FAIL : UD3TN_OK;

	ret = clock_gettime(CLOCK_REALTIME, &ts);
	ASSERT(ret != -1);
	ts.tv_sec += timeout_ms / 1000;
	ts.tv_nsec += (timeout_ms % 1000) * 1000000;

	// Ensure that the sum in tv_nsec is < 1 second in nanoseconds.
	if (ts.tv_nsec >= 1000000000) {
		ts.tv_sec += 1;
		ts.tv_nsec -= 1000000000;
		ASSERT(ts.tv_nsec < 1000000000);
	}

	do {
		ret = sem_timedwait(&sem->sem, &ts);
	} while (ret == -1 && errno == EINTR);

	return ret == -1 ? UD3TN_FAIL : UD3TN_OK;
}

#endif // __APPLE__

// Functions not specific to whether it is an Apple or non-Apple platform

Semaphore_t hal_semaphore_init_binary(void)
{
	Semaphore_t sem = malloc(sizeof(struct Semaphore));

	if (sem)
		hal_semaphore_init_inplace(sem, 0);
	return sem;
}

Semaphore_t hal_semaphore_init_value(int value)
{
	Semaphore_t sem = malloc(sizeof(struct Semaphore));

	if (sem)
		hal_semaphore_init_inplace(sem, value);
	return sem;
}
