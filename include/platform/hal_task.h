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
 * @brief hal_createTask Creates a new task in the underlying OS infrastructure
 * @param taskFunction Pointer to the initial task function
 *                     <b>currently this function should never reach its end</b>
 * @param taskName a descriptive name for the task
 * @param taskPriority The priority with witch the scheduler should regard
 *                     the task
 * @param taskParameters Arbitrary data that is passed to the created task
 * @param taskStackSize The number of words (not bytes!) to allocate for use
 *                      as the task's stack
 * @param taskTag Identifier that is assigned to the created task for
 *                debugging/tracing
 * @return a ud3tn_result indicating whether the operation was successful or not
 */
enum ud3tn_result hal_task_create(
	void (*task_function)(void *), const char *task_name,
	int task_priority, void *task_parameters,
	size_t task_stack_size);

/**
 * @brief hal_startScheduler Starts the task scheduler of the underlying OS
 *                           infrastructure (if necessary)
 */
void hal_task_start_scheduler(void);

/**
 * @brief hal_task_delay Blocks the calling task for the specified time.
 * @param delay The delay in milliseconds
 */
void hal_task_delay(int delay);


#endif /* HAL_TASK_H_INCLUDED */
