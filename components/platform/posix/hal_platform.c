// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_platform.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for platform-specific functionality
 *
 */

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_platform.h"
#include "platform/hal_time.h"
#include "platform/hal_task.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if LINUX_SPECIFIC_API
#include <malloc.h>
#endif

static char **restart_args;

static void exit_handler(int signal)
{
	if (signal == SIGHUP)
		fprintf(stderr, "SIGHUP detected, terminating\n");
	if (signal == SIGINT)
		fprintf(stderr, "SIGINT detected, terminating\n");
	if (signal == SIGTERM)
		fprintf(stderr, "SIGTERM detected, terminating\n");

	exit(EXIT_SUCCESS);
}

static void setup_exit_handler(void)
{
	struct sigaction sa;

	/* Setup the SIGHUP/SIGINT/SIGTERM handler */
	sa.sa_handler = &exit_handler;

	/* Restart the system call, if at all possible */
	sa.sa_flags = SA_RESTART;

	/* Block every signal during the handler */
	sigfillset(&sa.sa_mask);

	/* Intercept SIGHUP with this handler */
	if (sigaction(SIGHUP, &sa, NULL) == -1)
		LOG_ERRNO("HAL", "Error: cannot handle SIGHUP", errno);

	/* Intercept SIGINT with this handler */
	if (sigaction(SIGINT, &sa, NULL) == -1)
		LOG_ERRNO("HAL", "Error: cannot handle SIGINT", errno);

	/* Intercept SIGTERM with this handler	 */
	if (sigaction(SIGTERM, &sa, NULL) == -1)
		LOG_ERRNO("HAL", "Error: cannot handle SIGTERM", errno);

	// Ignore SIGPIPE so uD3TN does not crash if a connection is closed
	// during sending data. The event will be reported to us by the result
	// of the send(...) call.
	signal(SIGPIPE, SIG_IGN);
}

void hal_time_init(void); // not declared in public header

void hal_platform_init(int argc, char *argv[])
{
	setup_exit_handler();

	hal_io_init();
	hal_time_init(); // required for logging
	restart_args = malloc(sizeof(char *) * argc);
	if (restart_args) {
		// Copy all commandline args to the restart argument buffer
		for (int i = 1; i < argc; i++)
			restart_args[i - 1] = strdup(argv[i]);
		// NULL-terminate the array
		restart_args[argc - 1] = NULL;
	} else {
		LOG_ERROR("Error: Cannot allocate memory for restart buffer");
	}
}
