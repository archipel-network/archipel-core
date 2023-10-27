// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_time.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for semaphore-related functionality
 *
 */

#ifndef HAL_TIME_H_INCLUDED
#define HAL_TIME_H_INCLUDED

#include <stdint.h>

// 01/01/2000 00:00:00 UTC
#define DTN_TIMESTAMP_OFFSET 946684800

/**
 * @brief hal_time_get_timestamp_s Provides information about the current time
 * @return The current time in seconds
 */
uint64_t hal_time_get_timestamp_s(void);

/**
 * @brief hal_time_get_timestamp_ms Provides information about the current time
 * @return The current time in milliseconds
 */
uint64_t hal_time_get_timestamp_ms(void);

/**
 * @brief hal_time_get_timestamp_us Provides information about the current time
 * @return The current time in microseconds
 */
uint64_t hal_time_get_timestamp_us(void);

/**
 * @brief hal_time_get_system_time Provides the absolute system time (e.g.
 *				   uptime)
 * @return The uptime in microseconds
 */
uint64_t hal_time_get_system_time(void);

/**
 * @brief hal_time_print_log_time_string Print the system time via hal_io as
 *					 human-readable C-string.
 */
void hal_time_print_log_time_string(void);

#endif /* HAL_TIME_H_INCLUDED */
