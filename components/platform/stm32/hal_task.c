/*
 * hal_task.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for thread-related functionality
 *
 */

#include "platform/hal_task.h"

#include "ud3tn/common.h"

#include <FreeRTOS.h>
#include <task.h>

#include <stdlib.h>
#include <stdio.h>


Task_t hal_task_create(void (*task_function)(void *), const char *task_name,
		    int task_priority, void *task_parameters,
		    size_t task_stack_size, void *task_tag)
{
	TaskHandle_t cur_task = NULL;

	/* ensure that an actual function is provided for thread creation */
	ASSERT(task_function != NULL);

	/* create a new task in freeRTOS */
	BaseType_t returnValue = xTaskCreate(task_function, task_name,
					     task_stack_size, task_parameters,
					     task_priority, &cur_task);

	/* ensure the successful task creation before continuation */
	if (returnValue != pdPASS || cur_task == NULL)
		return NULL;

	/* tag the newly created task for tracing purposes */
	vTaskSetApplicationTaskTag(cur_task, task_tag);

	return cur_task;
}


void hal_task_start_scheduler(void)
{
	/* start the freeRTOS scheduler */
	/* no return value is provided */
	vTaskStartScheduler();
}


void hal_task_delay(int delay)
{
	vTaskDelay(delay / portTICK_PERIOD_MS);
}


void hal_task_delete(TaskHandle_t task)
{
	vTaskDelete(task);
}
