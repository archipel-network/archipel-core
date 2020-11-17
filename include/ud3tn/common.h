#ifndef UD3TN_COMMON_H_INCLUDED
#define UD3TN_COMMON_H_INCLUDED

/* POSIX functions provided in libc/Newlib headers */

// For strdup
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif // _POSIX_C_SOURCE
#include <string.h>

/* COMMON FUNCTIONS */

#define count_list_next_length(list, cnt) do { \
	if (list != NULL) \
		cnt = 1; \
	iterate_list_next(list) { ++cnt; } \
} while (0)

#define iterate_list_next(list) \
	for (; list; list = (list)->next)

#define list_element_free(list) do { \
	void *e = list->next; \
	free(list); \
	list = e; \
} while (0)

#define list_free(list) { \
while (list != NULL) { \
	list_element_free(list); \
} }

#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

#define MIN(a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_b < _a ? _b : _a; \
})
#define MAX(a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_b > _a ? _b : _a; \
})

#define HAS_FLAG(value, flag) ((value & flag) != 0)

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))
#define ARRAY_SIZE ARRAY_LENGTH

#if defined(__GNUC__) && (__GNUC__ >= 7) && !defined(__clang__)
#define fallthrough __attribute__ ((fallthrough))
#else
#define fallthrough
#endif

/* ASSERT */

#if defined(DEBUG)

#include <assert.h>

#define ASSERT(value) assert(value)

#else /* DEBUG */

// FIXME: A lot of cases are reported by clang-tidy in which an assertion may
// not be met, resulting in invalid pointers. Before they are fixed, this has
// to stay a check for all builds to ensure a reliable abort() if an
// assertion cannot be met.

// #define ASSERT(value) ((void)(value))

#include <assert.h>

#define ASSERT(value) assert(value)

#endif /* DEBUG */

#endif /* UD3TN_COMMON_H_INCLUDED */
