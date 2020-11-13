/*
 * hal_random.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for random number gerneration functionality
 *
 */

#define _POSIX_C_SOURCE 200809L

#include "platform/hal_random.h"

#include "util/bits.h"

#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <stdio.h>

void hal_random_init(void)
{
	/* initialize the random number generator with the current time */
	srand(time(0));
}

uint32_t hal_random_get(void)
{
	static __thread uint64_t leftover;
	static __thread uint_fast8_t leftover_bits;
	const size_t rand_bits = NBITS(RAND_MAX);

	while (leftover_bits < 32) {
		leftover |= ((uint64_t)rand()) << leftover_bits;
		leftover_bits = (leftover_bits + rand_bits) % 64;
	}

	uint32_t value = leftover & UINT32_MAX;

	leftover >>= 32;
	leftover_bits -= 32;
	return value;
}
