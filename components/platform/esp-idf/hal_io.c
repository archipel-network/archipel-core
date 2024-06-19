// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_io.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for input/output functionality
 *
 */

#include "platform/hal_time.h"
#include "platform/hal_semaphore.h"
#include "esp_log.h"

#include "ud3tn/result.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

enum ud3tn_result hal_io_init(void)
{
	return UD3TN_OK;
}

int hal_io_message_printf(const char *format, ...)
{
	va_list v;

	va_start(v, format);
	esp_log_write(1, "Archipel Core", format, v);
	va_end(v);
	return 1;
}

static inline const esp_log_level_t get_esp_log_level(const uint8_t level)
{
	switch (level) {
	case 1:
		return ESP_LOG_ERROR;
	case 2:
		return ESP_LOG_WARN;
	case 3:
		return ESP_LOG_INFO;
	default:
		return ESP_LOG_DEBUG;
	}
}

int hal_io_log_printf(const int level, const char *const file, const int line,
		      const char *const format, ...)
{
	va_list v;
	va_start(v, format);
	esp_log_write(get_esp_log_level(level), file, format, v);
	va_end(v);
	// TODO Implement line number in log
	//fprintf(stderr, " [%s:%d]\n", file, line);
	return 1;
}

void hal_io_log_perror(int level, const char *component, const char *file,
		       int line, const char *message, int error)
{
	hal_io_log_printf(
		1,
		file,
		line,
		"System error reported in %s - %s: %s [%s:%d]\n",
		component,
		message,
		strerror(error),
		file,
		line
	);
}
