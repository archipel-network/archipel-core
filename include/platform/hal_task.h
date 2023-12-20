// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_task.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for thread-related functionality
 *
 */

#ifndef HAL_TASK_H_INCLUDED
#define HAL_TASK_H_INCLUDED

#include "ud3tn/common.h"

#include "platform/hal_types.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

/**
 * @brief hal_task_create Creates a new task in the underlying OS.
 * @param task_function Pointer to the task function. When it terminates, the
 *                      task will terminate cleanly as well
 * @param task_parameters Arbitrary data that is passed to the created task
 * @return a ud3tn_result indicating whether the operation was successful or not
 */
enum ud3tn_result hal_task_create(
	void (*task_function)(void *),
	void *task_parameters);

/**
 * @brief hal_task_start_scheduler Starts the task scheduler of the underlying
 *                                 OS infrastructure (if necessary).
 */
void hal_task_start_scheduler(void);

/**
 * @brief hal_task_delay Blocks the calling task for the specified time.
 * @param delay The delay in milliseconds
 */
void hal_task_delay(int delay);


#endif /* HAL_TASK_H_INCLUDED */
