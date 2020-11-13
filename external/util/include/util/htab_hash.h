#ifndef HTAB_HASH_INCLUDED
#define HTAB_HASH_INCLUDED

#include <inttypes.h>
#include <stddef.h>

uint32_t hashlittle(const void *key, size_t length, uint32_t initval);

#endif /* HTAB_HASH_INCLUDED */
