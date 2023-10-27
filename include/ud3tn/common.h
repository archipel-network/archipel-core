// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef UD3TN_COMMON_H_INCLUDED
#define UD3TN_COMMON_H_INCLUDED

// We provide a public constant for that to allow compiling (and, thus,
// detecting errors in) debug code regardless of the #define.
#ifdef DEBUG
static const int IS_DEBUG_BUILD = 1;
#else // DEBUG
static const int IS_DEBUG_BUILD;
#endif // DEBUG

/* COMMON FUNCTIONS */

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

#define HAS_FLAG(value, flag) (((value) & (flag)) != 0)

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))
#define ARRAY_SIZE ARRAY_LENGTH

#if defined(__GNUC__) && (__GNUC__ >= 7) && !defined(__clang__)
#define fallthrough_ok __attribute__ ((fallthrough))
#else
#define fallthrough_ok
#endif

/* ASSERT */

#if defined(DEBUG)

#include <assert.h>

#define ASSERT(value) assert(value)

#else /* DEBUG */

#define ASSERT(value) ((void)(value))

#endif /* DEBUG */

#endif /* UD3TN_COMMON_H_INCLUDED */
