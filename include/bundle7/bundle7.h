#ifndef BUNDLE_V7_PARSER_H_INCLUDED
#define BUNDLE_V7_PARSER_H_INCLUDED

#include "ud3tn/bundle.h"
#include "ud3tn/result.h"

#include <inttypes.h>
#include <stddef.h>


/**
 * Returns the number of bytes that will be required for the CBORepresentation
 * of the passed unsigned integer.
 */
size_t bundle7_cbor_uint_sizeof(uint64_t num);


/**
 * Converts the unified uD3TN flags into BPv7-bis protocol-compliant block
 * processing flags.
 *
 * @return BPv7-bis bundle block processing flags
 */
uint16_t bundle7_convert_to_protocol_block_flags(
	const struct bundle_block *block);


/**
 * Returns the byte-length of the CBORepresentation of an extension block.
 */
size_t bundle7_block_get_size(struct bundle_block *block);

size_t bundle7_get_serialized_size(struct bundle *bundle);
size_t bundle7_get_serialized_size_without_payload(struct bundle *bundle);

/**
 * Recalculates the length of the primary block stored in the
 * "primary_block_length" field. You should call this function if you change
 * something in the primary block.
 */
void bundle7_recalculate_primary_block_length(struct bundle *bundle);

/**
 * Returns the minimal number of serialized bytes of the first fragment of the
 * given bundle.
 *
 * The minimal fragment contains:
 *
 *   - all extension blocks
 *   - minimal header for the payload block
 */
size_t bundle7_get_first_fragment_min_size(struct bundle *bundle);

/**
 * Returns the minimal number of serialized bytes of the last fragment of the
 * given bundle.
 *
 * The minimal fragment contains:
 *
 *   - all extension blocks containing the "replicated in every fragment" flag
 *   - minimal header for the payload block
 */
size_t bundle7_get_last_fragment_min_size(struct bundle *bundle);

#endif // BUNDLE_V7_PARSER_H_INCLUDED
