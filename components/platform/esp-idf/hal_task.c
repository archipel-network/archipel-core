// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_task.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for thread-related functionality
 *
 */

#include "ud3tn/common.h"

#include "platform/hal_io.h"
#include "platform/hal_task.h"
#include "platform/hal_time.h"

#include <freertos/task.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>

struct task_description {
	void (*task_function)(void *param);
	void *task_parameter;
};

static void *execute_pthread_compat(void *task_description)
{
	struct task_description *desc =
		(struct task_description *)task_description;
	void (*task_function)(void *param) = desc->task_function;
	void *task_parameter = desc->task_parameter;

	free(task_description);
	task_function(task_parameter);
	return NULL;
}

static unsigned int task_count = 0;

enum ud3tn_result hal_task_create(
	void (*task_function)(void *),
	void *task_parameters)
{
	char* name = malloc(sizeof(char) * 15);
	sprintf(name, "Task %d", task_count);
	task_count++;

	TaskHandle_t handle;

	xTaskCreate(
		task_function,
		name,
		25,
		task_parameters,
		1U,
		&handle
	);

	free(name);
}


__attribute__((noreturn))
void hal_task_start_scheduler(void)
{
	abort();
}


void hal_task_delay(int delay)
{
	if (delay < 0)
		return;

	vTaskDelay(pdMS_TO_TICKS(delay));
}
