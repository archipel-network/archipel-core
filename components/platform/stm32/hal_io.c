/*
 * hal_io.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for input/output functionality
 *
 */

#include "platform/hal_semaphore.h"

#include "ud3tn/config.h"
#include "ud3tn/result.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PRINTF_BUFFER_SIZE 192
static char printf_buffer[DEFAULT_PRINTF_BUFFER_SIZE];
static Semaphore_t printf_semaphore;

enum ud3tn_result hal_io_init(void)
{
	if (!IS_DEBUG_BUILD)
		return UD3TN_OK;

	printf_semaphore = hal_semaphore_init_binary();
	if (printf_semaphore == NULL)
		return UD3TN_FAIL;
	hal_semaphore_release(printf_semaphore);

	return UD3TN_OK;
}

static void write_to_debug(const char *const buffer)
{
	// SYS_WRITE0 (0x04):
	// Writes a null-terminated string to the debug channel.
	// On entry, R1 contains a pointer to the first byte of the string.
	asm volatile (
		"mov r0, #0x04\n" // SYS_WRITE0
		"mov r1, %[buf]\n"
		"bkpt #0xab\n"
			:
			: [buf] "r" (&printf_buffer) // inputs - ptr as reg
			: "r0", "r1" // clobber list (overwritten registers)
	);
}

int _hal_io_message_write(const char *const ptr, int length)
{
	if (!IS_DEBUG_BUILD)
		return 0;

	// Prevent concurrent access to the shared buffer.
	hal_semaphore_take_blocking(printf_semaphore);

	// String to be printed was too long, we indicate this with a tilde.
	if (length >= DEFAULT_PRINTF_BUFFER_SIZE - 1) {
		printf_buffer[DEFAULT_PRINTF_BUFFER_SIZE - 3] = '~';
		printf_buffer[DEFAULT_PRINTF_BUFFER_SIZE - 2] = '\n';
		length = DEFAULT_PRINTF_BUFFER_SIZE - 1;
	}
	printf_buffer[length] = 0;

	for (int i = 0; i < length; i++)
		printf_buffer[i] = ptr[i];

	write_to_debug(printf_buffer);

	hal_semaphore_release(printf_semaphore);

	return length;
}

int hal_io_message_printf(const char *format, ...)
{
	if (!IS_DEBUG_BUILD)
		return 0;

	va_list v;
	int length;

	// Prevent concurrent access to the shared buffer.
	hal_semaphore_take_blocking(printf_semaphore);

	va_start(v, format);
	// NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
	length = vsnprintf(
		printf_buffer, DEFAULT_PRINTF_BUFFER_SIZE,
		format, v);
	// String to be printed was too long, we indicate this with a tilde.
	if (length >= DEFAULT_PRINTF_BUFFER_SIZE - 1) {
		printf_buffer[DEFAULT_PRINTF_BUFFER_SIZE - 3] = '~';
		printf_buffer[DEFAULT_PRINTF_BUFFER_SIZE - 2] = '\n';
		length = DEFAULT_PRINTF_BUFFER_SIZE - 1;
	}
	printf_buffer[length] = 0;

	write_to_debug(printf_buffer);

	va_end(v);
	hal_semaphore_release(printf_semaphore);

	return length;
}
