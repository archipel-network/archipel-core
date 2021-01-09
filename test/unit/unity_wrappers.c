#include "platform/hal_io.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// NOTE: Please see build.mk - these wrappers are applied via LDFLAGS.

#ifdef PLATFORM_STM32
#define UNITTEST_DEBUG_BUF_LEN 256
static char unittest_debug_output_buffer[UNITTEST_DEBUG_BUF_LEN];
static int unittest_debug_output_pos;

static void unittest_putchar(const char c)
{
	unittest_debug_output_buffer[unittest_debug_output_pos++] = c;

	if (c == '\n' || c == '\0' ||
	    unittest_debug_output_pos >= UNITTEST_DEBUG_BUF_LEN - 1) {
		unittest_debug_output_buffer[unittest_debug_output_pos] = '\0';
		hal_io_message_printf("%s", unittest_debug_output_buffer);
		unittest_debug_output_pos = 0;
	}

}
#endif // PLATFORM_STM32

// Function to force Unity to use our debug interface for output.

void __wrap_UNITY_OUTPUT_CHAR(int c)
{
#ifdef PLATFORM_STM32
	unittest_putchar(c);
#else // PLATFORM_STM32
	putc(c, stdout);
#endif // PLATFORM_STM32
}

int __wrap_putchar(int c)
{
#ifdef PLATFORM_STM32
	unittest_putchar(c);
#else // PLATFORM_STM32
	putc(c, stdout);
#endif // PLATFORM_STM32
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
