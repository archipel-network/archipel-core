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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
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

enum ud3tn_result hal_task_create(
	void (*task_function)(void *),
	void *task_parameters)
{
	pthread_t thread;

	pthread_attr_t tattr;
	int error_code;
	struct task_description *desc = malloc(sizeof(*desc));

	if (desc == NULL) {
		LOG_ERROR("Allocating the task attribute structure failed!");
		goto fail;
	}

	/* initialize an attribute to the default value */
	if (pthread_attr_init(&tattr)) {
		/* abort if error occurs */
		LOG_ERROR("Initializing the task's attributes failed!");
		goto fail;
	}

	/* Create thread in detached state, so that no cleanup is necessary */
	if (pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED)) {
		LOG_ERROR("Setting detached state failed!");
		goto fail_attr;
	}

	desc->task_function = task_function;
	desc->task_parameter = task_parameters;

	error_code = pthread_create(&thread, &tattr,
				    execute_pthread_compat, desc);

	if (error_code) {
		LOG_ERROR("Thread Creation failed!");
		goto fail_attr;
	}

	/* destroy the attr-object */
	pthread_attr_destroy(&tattr);

	return UD3TN_OK;

fail_attr:
	/* destroy the attr-object */
	pthread_attr_destroy(&tattr);
fail:
	free(desc);

	return UD3TN_FAIL;
}


__attribute__((noreturn))
void hal_task_start_scheduler(void)
{
	/* Put the calling thread (in this case the main thread) to */
	/* sleep indefinitely */
	for (;;)
		pause();
}


void hal_task_delay(int delay)
{
	if (delay < 0)
		return;

	struct timespec req = {
		.tv_sec = delay / 1000,
		.tv_nsec = (delay % 1000) * 1000000
	};
	struct timespec rem;

	while (nanosleep(&req, &rem)) {
		if (errno != EINTR)
			break;
		req = rem;
	}
}
