/*
 * hal_semaphore.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for semaphore-related functionality
 *
 */

#include "platform/hal_semaphore.h"
#include "platform/hal_config.h"

#include "ud3tn/result.h"

#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <time.h>


Semaphore_t hal_semaphore_init_binary(void)
{
	Semaphore_t sem = malloc(sizeof(sem_t));

	if (sem)
		sem_init(sem, 0, 0);
	return sem;
}

void hal_semaphore_take_blocking(Semaphore_t sem)
{
	while (sem_wait(sem) == -1)
		;
}

void hal_semaphore_release(Semaphore_t sem)
{
	sem_post(sem);
}

void hal_semaphore_poll(Semaphore_t sem)
{
	hal_semaphore_try_take(sem, 0);
}

void hal_semaphore_delete(Semaphore_t sem)
{
	sem_destroy(sem);
	free(sem);
}

enum ud3tn_result hal_semaphore_try_take(Semaphore_t sem, int timeout_ms)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
		exit(EXIT_FAILURE);
	ts.tv_sec += timeout_ms/1000;
	ts.tv_nsec += (timeout_ms%1000)*1000000;
	if (sem_timedwait(sem, &ts) == -1)
		return UD3TN_FAIL;

	return UD3TN_OK;
}
