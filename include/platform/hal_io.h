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

#define LOGF(f_, ...) \
	hal_io_message_printf( \
		"[%s]: " f_ " [%s:%d]\n", \
		hal_time_get_log_time_string(), \
		__VA_ARGS__, \
		__FILE__, (int)(__LINE__) \
	)

#define LOG(message) LOGF("%s", message)
#define LOGI(message, itemid) LOGA(message, 0xFF, itemid)
#define LOGA(message, actionid, itemid) \
	LOGF("%s (a = %d, i = %d)", message, actionid, itemid)

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

#endif /* HAL_IO_H_INCLUDED */
