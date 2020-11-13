/*
 * hal_time.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for time-related functionality
 *
 */

#include "platform/hal_time.h"
#include "platform/hal_semaphore.h"

#include <hwf4/timer.h>

#include <FreeRTOS.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define hal_time_set_unix_timestamp(timestamp) \
	hal_time_init(timestamp - DTN_TIMESTAMP_OFFSET)
#define chal_time_get_unix_timestamp() \
	(hal_time_get_timestamp_s() + DTN_TIMESTAMP_OFFSET)
#define hal_time_get_unix_timestamp_ms() \
	(hal_time_get_timestamp_ms() + (DTN_TIMESTAMP_OFFSET * 1000))


static uint64_t ref_ticks;
static uint64_t ref_timestamp;

static char *time_string;
static Semaphore_t time_string_semph;

void hal_time_init(const uint64_t initial_timestamp)
{
	ref_ticks = timer_get_ticks();
	ref_timestamp = initial_timestamp;
}

uint64_t hal_time_get_timestamp_s(void)
{
	uint64_t tick_difference = timer_get_ticks() - ref_ticks;

	return ref_timestamp + (uint64_t)(tick_difference / 1000000ULL);
}

uint64_t hal_time_get_timestamp_ms(void)
{
	uint64_t tick_difference = timer_get_ticks() - ref_ticks;

	return ref_timestamp * 1000ULL + (uint64_t)(tick_difference / 1000ULL);
}


uint64_t hal_time_get_timestamp_us(void)
{
	return ref_timestamp * 1000000ULL + timer_get_ticks() - ref_ticks;
}


uint64_t hal_time_get_system_time(void)
{
	return timer_get_ticks();
}

char *hal_time_get_log_time_string(void)
{
	char *tmp_string;

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

	return tmp_string;
}
