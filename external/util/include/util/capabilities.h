// x86 and x86-64 features
#ifndef CAPABILITIES_H
#define CAPABILITIES_H

#if defined(__PCLMUL__) && __PCLMUL__ == 1
#  define HAVE_PCLMUL 1
#else
#  define HAVE_PCLMUL 0
#endif

// ARM features

#if defined(__ARM_NEON__) && __ARM_NEON__ == 1 || \
    defined(__ARM_NEON) && __ARM_NEON == 1
#  define HAVE_NEON 1
#else
#  define HAVE_NEON 0
#endif

// GCC features

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#  define HAVE_BUILTIN_CLZ 1
#else
#  define HAVE_BUILTIN_CLZ 0
#endif

#endif // CAPABILITIES_H
