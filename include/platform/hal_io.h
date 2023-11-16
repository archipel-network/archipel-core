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

#include "ud3tn/result.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern uint8_t LOG_LEVEL;

#define LOGF_GENERIC(level, f_, ...) \
do { \
	__typeof__(level) level_ = (level); \
	if (level_ <= LOG_LEVEL) { \
		hal_io_log_printf( \
			level_, \
			__FILE__, \
			(int)(__LINE__), \
			f_, \
			__VA_ARGS__ \
		); \
	} \
} while (0)

#define LOGF_ERROR(...) LOGF_GENERIC(1, __VA_ARGS__)
#define LOGF_WARN(...) LOGF_GENERIC(2, __VA_ARGS__)
#define LOGF_INFO(...) LOGF_GENERIC(3, __VA_ARGS__)
#define LOGF LOGF_INFO

#define LOG_ERROR(message) LOGF_ERROR("%s", message)
#define LOG_WARN(message) LOGF_WARN("%s", message)
#define LOG_INFO(message) LOGF("%s", message)
#define LOG LOG_INFO

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

#define LOG_ERRNO(component_, msg_, errno_) \
	hal_io_log_perror( \
		component_, \
		__FILE__, \
		(int)(__LINE__), \
		msg_, \
		errno_ \
	)
#define LOGERROR LOG_ERRNO

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
 * @brief hal_io_log_printf Write a log message to the debug interface.
 * @param level The log level - a value between 1 (ERROR) and 4 (DEBUG).
 * @param file The file that triggered the log message.
 * @param line The line that triggered the log message.
 * @param format Same as in standard libc-printf().
 */
int hal_io_log_printf(int level, const char *file, int line,
		      const char *format, ...);

/**
 * @brief hal_io_log_perror Log a system error (i.e., saved errno).
 * @param component The component in which the error occurred, e.g., "Router".
 * @param file The file in which the error occurred.
 * @param line The line in which the error occurred.
 * @param message The message passed to perror().
 * @param error The error number obtained from errno.
 */
void hal_io_log_perror(const char *component, const char *file, int line,
		       const char *message, int error);

#endif /* HAL_IO_H_INCLUDED */
