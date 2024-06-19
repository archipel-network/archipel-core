// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_time.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for time-related functionality
 *
 */

#include "platform/hal_time.h"
#include "platform/hal_semaphore.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

static Semaphore_t time_string_semph;

void hal_time_init(void)
{
	time_string_semph = hal_semaphore_init_binary();
	hal_semaphore_release(time_string_semph);
}

uint64_t hal_time_get_timestamp_s(void)
{
	struct timeval tv;

	/* returns the time relative to the unix epoch */
	gettimeofday(&tv, NULL);

	/* we want to use the DTN epoch -> subtract the offset */
	return tv.tv_sec - DTN_TIMESTAMP_OFFSET;
}

uint64_t hal_time_get_timestamp_ms(void)
{
	struct timeval tv;
	unsigned long ms; /* Milliseconds */
	time_t s;  /* Seconds */

	/* returns the time relative to the unix epoch */
	gettimeofday(&tv, NULL);

	/* We want to use the DTN epoch -> subtract the offset */
	s = tv.tv_sec - DTN_TIMESTAMP_OFFSET;
	/* Convert microseconds to milliseconds */
	ms = (tv.tv_usec + 500) / 1000;

	return ((uint64_t)s * 1000) + (uint64_t)ms;
}


uint64_t hal_time_get_timestamp_us(void)
{
	struct timeval tv;
	unsigned long us; /* Microseconds */
	time_t s;  /* Seconds */

	/* returns the time relative to the unix epoch */
	gettimeofday(&tv, NULL);

	/* We want to use the DTN epoch -> subtract the offset */
	s = tv.tv_sec - DTN_TIMESTAMP_OFFSET;
	/* Convert nanoseconds to microseconds */
	us = tv.tv_usec + 5;

	return ((uint64_t)s * 1000000) + (uint64_t)us;
}


uint64_t hal_time_get_system_time(void)
{
	return hal_time_get_timestamp_us();
}


void hal_time_print_log_time_string(void)
{
	// Not implemented for 
}