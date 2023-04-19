// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef HAL_TYPES_H_INCLUDED
#define HAL_TYPES_H_INCLUDED

#include "platform/posix/simple_queue.h"

#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>

#ifdef __APPLE__

#include <dispatch/dispatch.h>

typedef struct Semaphore {
	dispatch_semaphore_t sem;
} *Semaphore_t;

#else // __APPLE__

#include <semaphore.h>

typedef struct Semaphore {
	sem_t sem;
} *Semaphore_t;

#endif // __APPLE__

#define QueueIdentifier_t Queue_t*

#endif // HAL_TYPES_H_INCLUDED
