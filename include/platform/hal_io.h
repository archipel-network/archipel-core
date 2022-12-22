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

#define LOGF(f_, ...) ({ \
	hal_time_print_log_time_string(); \
	hal_io_message_printf( \
		f_ " [%s:%d]\n", \
		__VA_ARGS__, \
		__FILE__, (int)(__LINE__) \
	); \
	fflush(stderr); \
})

#define LOG(message) LOGF("%s", message)
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
