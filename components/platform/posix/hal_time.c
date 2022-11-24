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
#include <time.h>

static uint64_t ref_timestamp;
static bool mod_time;
static struct timespec ref_timespec;

static Semaphore_t time_string_semph;

void hal_time_init(const uint64_t initial_timestamp)
{
	/* get the current system time */
	clock_gettime(CLOCK_REALTIME, &ref_timespec);

	if (initial_timestamp != UINT64_MAX) {
		/* calculate offset to required time */
		ref_timestamp = ref_timespec.tv_sec - DTN_TIMESTAMP_OFFSET -
				initial_timestamp;
		mod_time = true;
	} else {
		/* reset to system time */
		ref_timestamp = 0;
		mod_time = false;
	}

	time_string_semph = hal_semaphore_init_binary();
	hal_semaphore_release(time_string_semph);
}

uint64_t hal_time_get_timestamp_s(void)
{
	struct timespec ts;

	/* returns the time relative to the unix epoch */
	clock_gettime(CLOCK_REALTIME, &ts);

	/* we want to use the DTN epoch -> subtract the offset */
	return ts.tv_sec - DTN_TIMESTAMP_OFFSET - ref_timestamp;
}

uint64_t hal_time_get_timestamp_ms(void)
{
	struct timespec ts;
	long ms; /* Milliseconds */
	time_t s;  /* Seconds */

	/* Returns the time relative to the unix epoch */
	clock_gettime(CLOCK_REALTIME, &ts);

	/* We want to use the DTN epoch -> subtract the offset */
	s = ts.tv_sec - DTN_TIMESTAMP_OFFSET - ref_timestamp;
	/* Convert nanoseconds to milliseconds */
	ms = round(ts.tv_nsec / 1.0e6);

	return (s*1.0e3)+ms;
}


uint64_t hal_time_get_timestamp_us(void)
{
	struct timespec ts;
	long us; /* Milliseconds */
	time_t s;  /* Seconds */

	/* Returns the time relative to the unix epoch */
	clock_gettime(CLOCK_REALTIME, &ts);

	/* We want to use the DTN epoch -> subtract the offset */
	s = ts.tv_sec - DTN_TIMESTAMP_OFFSET - ref_timestamp;
	/* Convert nanoseconds to microseconds */
	us = round(ts.tv_nsec / 1.0e3);

	return (s*1.0e6)+us;
}


uint64_t hal_time_get_system_time(void)
{
	return hal_time_get_timestamp_us();
}


void hal_time_print_log_time_string(void)
{
	time_t t;
	char *timestr;

	hal_semaphore_take_blocking(time_string_semph);
	if (!mod_time) {
		time(&t);
		timestr = ctime(&t);
		timestr[strlen(timestr) - 1] = '\0'; // remove '\n'
		fprintf(stderr, "[%s] ", timestr);
	} else {
		fprintf(stderr, "[%llu ms] ",
			(unsigned long long)hal_time_get_timestamp_ms());
	}
	hal_semaphore_release(time_string_semph);
}
