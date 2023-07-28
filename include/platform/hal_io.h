// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_debug.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for input/output functionality
 *
 */

#ifndef HAL_IO_H_INCLUDED
#define HAL_IO_H_INCLUDED

#include "platform/hal_time.h"

#include "ud3tn/result.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern uint8_t LOG_LEVEL;

static inline const char *get_log_level_name(const uint8_t level)
{
	switch (level) {
	case 1:
		return "ERROR";
	case 2:
		return "WARNING";
	case 3:
		return "INFO";
	default:
		return "DEBUG";
	}
}

#define LOGF_GENERIC(level, f_, ...) \
({ \
	__typeof__(level) _level = (level); \
	if (_level <= LOG_LEVEL) { \
		hal_time_print_log_time_string(); \
		hal_io_message_printf( \
			"[%s] " f_ " [%s:%d]\n", \
			get_log_level_name(_level), \
			__VA_ARGS__, \
			__FILE__, (int)(__LINE__) \
		); \
		fflush(stderr); \
	} \
})

#define LOGF(...) LOGF_GENERIC(3, __VA_ARGS__)
#define LOGF_ERROR(...) LOGF_GENERIC(1, __VA_ARGS__)
#define LOGF_WARN(...) LOGF_GENERIC(2, __VA_ARGS__)

#define LOG(message) LOGF("%s", message)
#define LOG_ERROR(message) LOGF_ERROR("%s", message)
#define LOG_WARN(message) LOGF_WARN("%s", message)

#ifdef DEBUG
#define LOGF_DEBUG(...) LOGF_GENERIC(4, __VA_ARGS__)
#define LOG_DEBUG(message) LOGF_DEBUG("%s", message)
#else /* DEBUG */
#define LOGF_DEBUG(...) ((void)0)
#define LOG_DEBUG(message) ((void)0)
#endif /* DEBUG */

#define LOGI(message, itemid) LOGA(message, 0xFF, itemid)
#define LOGA(message, actionid, itemid) \
	LOGF("%s (a = %d, i = %d)", message, actionid, itemid)

#define LOGERROR(component_, msg_, errno_) ({ \
	hal_time_print_log_time_string(); \
	hal_io_message_printf( \
		"%s: System error [%s:%d] -> ", \
		component_, __FILE__, (int)(__LINE__) \
	); \
	hal_io_print_error(msg_, errno_); \
	fflush(stderr); \
})

/**
 * @brief hal_io_init Initialization of underlying OS/HW for I/O
 * @return Whether the operation was successful
 */
enum ud3tn_result hal_io_init(void);

/**
 * @brief hal_io_message_printf Write a string with arbitrary length to the
 *				debug interface, provides functionality as
 *				libc-printf()
 * @param format Parameters as standard libc-printf()
 */
int hal_io_message_printf(const char *format, ...);

/**
 * @brief hal_io_perror Print a system error (i.e., saved errno) using perror()
 * @param message The message passed to perror()
 * @param error The error number obtained from errno
 */
void hal_io_print_error(const char *message, int error);

#endif /* HAL_IO_H_INCLUDED */
