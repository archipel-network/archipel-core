#ifndef UTIL_BITS_H_INCLUDED
#define UTIL_BITS_H_INCLUDED

#include "util/capabilities.h"

#if HAVE_BUILTIN_CLZ
#  define NBITS(n) ((int) (sizeof(unsigned long long) * CHAR_BIT \
                           - ((n) > 0 ? __builtin_clzll(n) : 63))  )

#else

#  define NBITS(n)    (NBITS_64((uint64_t) n) + 1)
#  define NBITS_64(n) (((n) & 0xFFFFFFFF00000000) ?   \
                       (32 + NBITS_32((n) >> 32)) : NBITS_32(n))
#  define NBITS_32(n) (((n) &         0xFFFF0000) ?   \
                       (16 + NBITS_16((n) >> 16)) : NBITS_16(n))
#  define NBITS_16(n) (((n) &             0xFF00) ?   \
                       (8  + NBITS_8( (n) >>  8)) : NBITS_8(n))
#  define NBITS_8(n)  (((n) &               0xF0) ?   \
                       (4  + NBITS_4( (n) >>  4)) : NBITS_4(n))
#  define NBITS_4(n)  (((n) &                0xC) ?   \
                       (2  + NBITS_2( (n) >>  2)) : NBITS_2(n))
#  define NBITS_2(n)  (((n) &                0x2) ?   \
                        1                         : 0)
#endif

#endif // UTIL_BITS_H_INCLUDED
