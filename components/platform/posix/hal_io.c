// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_io.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for input/output functionality
 *
 */

#include "ud3tn/result.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

enum ud3tn_result hal_io_init(void)
{
	return UD3TN_OK;
}

int hal_io_message_printf(const char *format, ...)
{
	int rc;
	va_list v;

	va_start(v, format);
	// A false positive error of clang-tidy follows...
	// NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
	rc = vfprintf(stderr, format, v);
	va_end(v);
	return rc;
}

void hal_io_print_error(const char *message, int error)
{
	errno = error;
	perror(message);
}
