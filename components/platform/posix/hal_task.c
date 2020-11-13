/*
 * hal_task.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for thread-related functionality
 *
 */


/* enable linux features when linux OS */
#if linux
#define _GNU_SOURCE
#endif

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_task.h"
#include "platform/hal_time.h"

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

Task_t hal_task_create(void (*task_function)(void *), const char *task_name,
		       int task_priority, void *task_parameters,
		       size_t task_stack_size, void *task_tag)
{
	pthread_t *thread = malloc(sizeof(pthread_t));

	if (thread == NULL)
		return NULL;

	struct sched_param param;
	pthread_attr_t tattr;
	int error_code;
	struct task_description *desc = malloc(sizeof(*desc));

	if (desc == NULL) {
		LOG("Allocating the task attribute structure failed!");
		goto fail;
	}

	/* initialize an attribute to the default value */
	if (pthread_attr_init(&tattr)) {
		/* abort if error occurs */
		LOG("Initializing the task's attributes failed!");
		goto fail;
	}

	/* set the scheduling policy */
	if (pthread_attr_setschedpolicy(&tattr, SCHED_RR)) {
		/* abort if error occurs */
		LOG("Setting the scheduling policy failed!");
		goto fail_attr;
	}

	/* Create thread in detached state, so that no cleanup is necessary */
	if (pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED)) {
		LOG("Setting detached state failed!");
		goto fail_attr;
	}

	/* set the scheduling priority (just use the absolute minimum and add */
	/* the given priority */
	param.sched_priority = sched_get_priority_min(SCHED_RR) +
			task_priority;
	if (pthread_attr_setschedparam(&tattr, &param)) {
		/* abort if error occurs */
		LOG("Setting the scheduling priority failed!");
		goto fail_attr;
	}

	/* update the stack size of the thread (only if greater than 0, */
	/* otherwise set stack size to the default value */
	if (task_stack_size != 0 &&
			pthread_attr_setstacksize(&tattr, task_stack_size)) {
		/* abort if error occurs */
		LOG("Setting the tasks stack size failed! Wrong value!");
		goto fail_attr;
	}

	desc->task_function = task_function;
	desc->task_parameter = task_parameters;

	error_code = pthread_create(thread, &tattr,
				    execute_pthread_compat, desc);

	if (error_code) {
		LOG("Thread Creation failed!");
		goto fail_attr;
	}

#if LINUX_SPECIFIC_API
	if (pthread_setname_np(thread, task_name)) {
		LOG("Could not set thread name!");
		goto fail_attr;
	}
#endif

	/* destroy the attr-object */
	pthread_attr_destroy(&tattr);

	return thread;

fail_attr:
	/* destroy the attr-object */
	pthread_attr_destroy(&tattr);
fail:
	free(thread);
	free(desc);

	return NULL;
}


void hal_task_start_scheduler(void)
{
	/* Put the calling thread (in this case the main thread) to */
	/* sleep indefinitely */
	pause();
}


void hal_task_delay(int delay)
{
	usleep(delay*1000);
}


void hal_task_delete(Task_t task)
{
	pthread_cancel(*task);
	free(task);
}
