/*
 * hal_time.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for time-related functionality
 *
 */

#include "platform/hal_time.h"
#include "platform/hal_semaphore.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <string.h>

static uint64_t ref_timestamp;
static bool mod_time;
static struct timespec ref_timespec;

static char *time_string;
static Semaphore_t time_string_semph;

void hal_time_init(const uint64_t initial_timestamp)
{
	/* get the current system time */
	clock_gettime(CLOCK_REALTIME, &ref_timespec);

	/* calculate offset to required time */
	ref_timestamp = ref_timespec.tv_sec - DTN_TIMESTAMP_OFFSET -
			initial_timestamp;

	mod_time = (initial_timestamp != 0);
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


char *hal_time_get_log_time_string(void)
{
	char *tmp_string;
	time_t t;

	if (!mod_time) {
		time(&t);
		tmp_string = ctime(&t);
		/* remove \n from the end of the string */
		tmp_string[strlen(tmp_string) - 1] = '\0';
	} else {
		if (time_string == NULL) {
			time_string_semph = hal_semaphore_init_binary();
			time_string = malloc(64);
			time_string[63] = '\0';
		} else {
			hal_semaphore_take_blocking(time_string_semph);
		}

		snprintf(time_string, 64, "%llu",
			(unsigned long long) hal_time_get_timestamp_s());

		hal_semaphore_release(time_string_semph);

		tmp_string = time_string;
	}

	return tmp_string;
}
