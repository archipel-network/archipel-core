/*
 * hal_task.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for thread-related functionality
 *
 */

#ifndef HAL_TASK_H_INCLUDED
#define HAL_TASK_H_INCLUDED

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
 * @return a handle for the newly created task
 */
Task_t hal_task_create(void (*taskFunction)(void *), const char *taskName,
		      int taskPriority, void *taskParameters,
		      size_t taskStackSize, void *taskTag);

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

/**
 * @brief hal_task_delete Kills and deletes the given task
 * @param task The task that should be killed. If parameter is NULL, the
 *	       calling task is killed
 */
void hal_task_delete(Task_t task);


#endif /* HAL_TASK_H_INCLUDED */
