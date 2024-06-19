// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_platform.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for platform-specific functionality
 *
 */

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

static char **restart_args;

static void exit_handler(int signal)
{
	// Exit handler not supported on ESP
}

static void setup_exit_handler(void)
{
	// Exit handler not supported on ESP
}

void hal_time_init(void); // not declared in public header

void hal_platform_init(int argc, char *argv[])
{
	setup_exit_handler();

	hal_io_init();
	hal_time_init(); // required for logging
}
