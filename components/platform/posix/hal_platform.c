/*
 * hal_platform.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for platform-specific functionality
 *
 */

#include "platform/hal_config.h"
#include "platform/hal_crypto.h"
#include "platform/hal_io.h"
#include "platform/hal_platform.h"
#include "platform/hal_random.h"
#include "platform/hal_time.h"
#include "platform/hal_task.h"

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
		LOG("SIGHUP detected, terminating");
	if (signal == SIGINT)
		LOG("SIGINT detected, terminating");
	if (signal == SIGTERM)
		LOG("SIGTERM detected, terminating");

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
		LOG("Error: cannot handle SIGHUP"); /* Should not happen */

	/* Intercept SIGINT with this handler */
	if (sigaction(SIGINT, &sa, NULL) == -1)
		LOG("Error: cannot handle SIGINT"); /* Should not happen */

	/* Intercept SIGTERM with this handler	 */
	if (sigaction(SIGTERM, &sa, NULL) == -1)
		LOG("Error: cannot handle SIGTERM"); /* Should not happen */

	// Ignore SIGPIPE so uD3TN does not crash if a connection is closed
	// during sending data. The event will be reported to us by the result
	// of the send(...) call.
	signal(SIGPIPE, SIG_IGN);
}

void hal_platform_led_pin_set(uint8_t led_identifier, int mode)
{
	/* not relevant for the POSIX implementation */
}


void hal_platform_led_set(int led_preset)
{
	/* not relevant for the POSIX implementation */
}

void mpu_init(void)
{
	/* currently not relevant for the POSIX implementation */
}


void hal_platform_init(int argc, char *argv[])
{
	setup_exit_handler();

	restart_args = malloc(sizeof(char *) * argc);
	if (restart_args) {
		// Copy all commandline args to the restart argument buffer
		for (int i = 1; i < argc; i++)
			restart_args[i - 1] = strdup(argv[i]);
		// NULL-terminate the array
		restart_args[argc - 1] = NULL;
	} else {
		LOG("Error: Cannot allocate memory for restart buffer");
	}
}

__attribute__((noreturn))
void hal_platform_restart(void)
{
	// TODO: Try to close open ports (e.g. TCP)
	LOG("Restarting!");

	// If restart_args could not be allocated, this is used (no arguments)
	char *const backup_restart_buf[1] = {NULL};

	if (restart_args)
		execv("/proc/self/exe", restart_args);
	else
		execv("/proc/self/exe", backup_restart_buf);

	__builtin_unreachable();
}
