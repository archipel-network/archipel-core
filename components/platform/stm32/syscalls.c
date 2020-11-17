// Implementation of Newlib syscalls.
// See: https://sourceware.org/newlib/libc.html#Stubs

#include "platform/hal_platform.h"
#include "platform/hal_time.h"

#include "ud3tn/config.h"

#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>

#include <stm32f4xx.h>

#include <FreeRTOS.h>
#include <task.h>

#include <signal.h>
#include <stdlib.h>

#include <errno.h>
#undef errno
extern int errno;

extern int _end;
extern int _heap_end;

char *__env[1] = { 0 };
char **environ = __env;

int _hal_io_message_write(const char *const ptr, int length);

caddr_t _sbrk(int incr)
{
	static unsigned char *cur_end = NULL, *heap_end;
	unsigned char *prev_end, *tmp;

	if (cur_end == NULL) {
		cur_end = (unsigned char *)&_end;
		heap_end = (unsigned char *)&_heap_end;
	}
	prev_end = cur_end;
	tmp = cur_end + incr;
	if (tmp >= heap_end) {
		errno = ENOMEM;
		return NULL;
	}
	cur_end = tmp;
	return (caddr_t)prev_end;
}

int _link(char *old, char *new)
{
	(void)old;
	(void)new;
	errno = EMLINK;
	return -1;
}

int _unlink(char *name)
{
	(void)name;
	errno = ENOENT;
	return -1;
}

int _open(const char *name, int flags, int mode)
{
	(void)name;
	(void)flags;
	(void)mode;
	return -1;
}

int _close(int file)
{
	(void)file;
	return -1;
}

int _fstat(int file, struct stat *st)
{
	(void)file;
	(void)st;
	st->st_mode = S_IFCHR;
	return 0;
}

int _stat(int file, struct stat *st)
{
	(void)file;
	(void)st;
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file)
{
	(void)file;
	return 1;
}

int _lseek(int file, int ptr, int dir)
{
	(void)file;
	(void)ptr;
	(void)dir;
	return 0;
}

int _read(int file, char *ptr, int len)
{
	(void)file;
	(void)ptr;
	(void)len;
	return 0;
}

int _write(int file, char *ptr, int len)
{
	(void)file;
	(void)ptr;
	(void)len;
	return _hal_io_message_write(ptr, len);
}

int _execve(char *name, char **argv, char **env)
{
	(void)name;
	(void)argv;
	(void)env;
	errno = ENOMEM;
	return -1;
}

int _fork(void)
{
	errno = EAGAIN;
	return -1;
}

__attribute__((noreturn))
void _exit(int status)
{
	if (status == EXIT_SUCCESS)
		hal_platform_led_set(1);
	else
		hal_platform_led_set(5);

	if (IS_DEBUG_BUILD) {
		// In case of a debug build we want to issue a breakpoint
		asm volatile ("bkpt");
	}

	vTaskSuspendAll();

	// Wait for 3 seconds until we restart the SoC...
	uint64_t t0 = hal_time_get_timestamp_ms();

	while (hal_time_get_timestamp_ms() < t0 + 3000)
		;

	NVIC_SystemReset();
	__builtin_unreachable();
}

int _kill(int pid, int sig)
{
	if (pid <= 1) {
		if (sig == SIGTERM)
			_exit(EXIT_SUCCESS);
		else
			_exit(EXIT_FAILURE);
	}
	errno = EINVAL;
	return -1;
}

int _getpid(void)
{
	return 1;
}

int _times(struct tms *buf)
{
	(void)buf;
	return -1;
}

int _wait(int *status)
{
	(void)status;
	errno = ECHILD;
	return -1;
}
