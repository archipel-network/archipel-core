// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "platform/hal_io.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// NOTE: Please see build.mk - these wrappers are applied via LDFLAGS.

// Function to force Unity to use our debug interface for output.

void __wrap_UNITY_OUTPUT_CHAR(int c)
{
	putc(c, stdout);
}

int __wrap_putchar(int c)
{
	putc(c, stdout);
	return 0;
}

// Wrappers for the Unity allocator wrappers to be able to normally use
// standard allocation functions without including the unity stuff everywhere.

void *__wrap_unity_malloc(size_t size)
{
	return malloc(size);
}

void *__wrap_unity_calloc(size_t num, size_t size)
{
	return calloc(num, size);
}

void *__wrap_unity_realloc(void *oldMem, size_t size)
{
	return realloc(oldMem, size);
}

void __wrap_unity_free(void *mem)
{
	free(mem);
}
