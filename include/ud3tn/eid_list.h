#ifndef EIDLIST_H_INCLUDED
#define EIDLIST_H_INCLUDED

#include <inttypes.h>

char **eidlist_decode(const uint8_t *const ptr, const int length);
uint8_t *eidlist_encode(
	const char *const *const eids, const uint8_t count, int *const res_len);

#endif /* EIDLIST_H_INCLUDED */
