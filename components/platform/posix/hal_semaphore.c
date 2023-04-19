// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_semaphore.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for semaphore-related functionality
 *
 */

#include "platform/hal_config.h"
#include "platform/hal_semaphore.h"
#include "platform/posix/hal_types.h"

#include "ud3tn/common.h"
#include "ud3tn/result.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else // __APPLE__
#include <semaphore.h>
#endif // __APPLE__

// https://stackoverflow.com/a/27847103
// not exported but used in simple_queue for performance reasons
void hal_semaphore_init_inplace(Semaphore_t sem, int value)
{
#ifdef __APPLE__
	sem->sem = dispatch_semaphore_create(value);
#else
	sem_init(&sem->sem, 0, value);
#endif
}

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

void hal_semaphore_take_blocking(Semaphore_t sem)
{
#ifdef __APPLE__
	dispatch_semaphore_wait(sem->sem, DISPATCH_TIME_FOREVER);
#else // __APPLE__
	int ret;

	do {
		ret = sem_wait(&sem->sem);
	} while (ret == -1 && errno == EINTR);
#endif // __APPLE__
}

void hal_semaphore_release(Semaphore_t sem)
{
#ifdef __APPLE__
	dispatch_semaphore_signal(sem->sem);
#else // __APPLE__
	sem_post(&sem->sem);
# endif // __APPLE__
}

bool hal_semaphore_is_blocked(Semaphore_t sem)
{
#ifdef __APPLE__
	if (dispatch_semaphore_wait(sem->sem, DISPATCH_TIME_NOW) == 0) {
		dispatch_semaphore_signal(sem->sem);
		return 0;
	}
	return 1;
#else // __APPLE__
	int rv = 0;

	sem_getvalue(&sem->sem, &rv);

	return rv == 0;
# endif // __APPLE__
}

void hal_semaphore_delete(Semaphore_t sem)
{
#ifdef __APPLE__
	sem->sem = NULL;
#else // __APPLE__
	sem_destroy(&sem->sem);
# endif // __APPLE__
	free(sem);
}

enum ud3tn_result hal_semaphore_try_take(Semaphore_t sem, int timeout_ms)
{
	if (timeout_ms < 0) {
		hal_semaphore_take_blocking(sem);
		return UD3TN_OK;
	}

#ifdef __APPLE__
	dispatch_time_t dt = dispatch_time(
		DISPATCH_TIME_NOW,
		timeout_ms * 1000000
	);

	return dispatch_semaphore_wait(sem->sem, dt) ? UD3TN_FAIL : UD3TN_OK;
#else // __APPLE__
	int ret;
	struct timespec ts;

	if (timeout_ms == 0)
		return sem_trywait(&sem->sem) == -1 ? UD3TN_FAIL : UD3TN_OK;

	ret = clock_gettime(CLOCK_REALTIME, &ts);
	ASSERT(ret != -1);
	ts.tv_sec += timeout_ms / 1000;
	ts.tv_nsec += (timeout_ms % 1000) * 1000000;

	do {
		ret = sem_timedwait(&sem->sem, &ts);
	} while (ret == -1 && errno == EINTR);

	return ret == -1 ? UD3TN_FAIL : UD3TN_OK;
# endif // __APPLE__
}
