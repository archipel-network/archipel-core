#ifndef BUNDLEFRAGMENTER_H_INCLUDED
#define BUNDLEFRAGMENTER_H_INCLUDED

#include "ud3tn/bundle.h"
#include "ud3tn/node.h"

#include <stdbool.h>
#include <stdint.h>


/**
 * RFC 5050
 * --------
 *
 * BPv7-bis
 * --------
 * @param bundle [description]
 * @return [description]
 */
struct bundle *bundlefragmenter_initialize_first_fragment(struct bundle *input);


struct bundle *bundlefragmenter_fragment_bundle(
	struct bundle *working_bundle, uint64_t first_max);


/**
 * Creates a new fragment for the given bundle
 *
 * @param init_payload If true, any existing payload block will be freed and a
 *                     new payload block with size zero will be created
 *
 * @return in case if any error NULL will be returned, otherwise the newly
 *         created fragment
 */
struct bundle *bundlefragmenter_create_new_fragment(
	struct bundle const *prototype, bool init_payload);


#endif /* BUNDLEFRAGMENTER_H_INCLUDED */
