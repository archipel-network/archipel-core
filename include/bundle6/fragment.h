#ifndef BUNDLE6_FRAGMENT_H_INCLUDED
#define BUNDLE6_FRAGMENT_H_INCLUDED

#include "ud3tn/bundle.h"

#include <stdint.h>

struct bundle *bundle6_initialize_first_fragment(struct bundle *input);

struct bundle *bundle6_fragment_bundle(
	struct bundle *working_bundle,
	uint64_t first_max);

#endif /* BUNDLE6_FRAGMENT_H_INCLUDED */
