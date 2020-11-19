#ifndef BUNDLE7_FRAGMENT_H_INCLUDED
#define BUNDLE7_FRAGMENT_H_INCLUDED

#include "ud3tn/bundle.h"

#include <stdint.h>

struct bundle *bundle7_fragment_bundle(struct bundle *working_bundle,
	uint64_t first_max);

#endif // BUNDLE7_FRAGMENT_H_INCLUDED
