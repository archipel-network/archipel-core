#ifndef BUNDLE7_HOPCOUNT_H_INCLUDED
#define BUNDLE7_HOPCOUNT_H_INCLUDED

#include "ud3tn/bundle.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/**
 * Parses the given CBOR-encoded data into a Hop Count structure.
 * This function can be used to parse the payload of an Hop Count
 * bundle extension block.
 *
 * @param buffer CBOR-encoded data
 * @param length CBOR data length
 */
bool bundle7_hop_count_parse(struct bundle_hop_count *hop_count,
	const uint8_t *buffer, size_t length);


/**
 * Maximal length of a CBOR-encoded Hop Count
 *
 * It is calculated as follows:
 *
 *  - 1 Byte CBOR array header
 *  - 3 Bytes uint16_t limit
 *  - 3 Bytes uint16_t count
 */
#define BUNDLE7_HOP_COUNT_MAX_ENCODED_SIZE 7


/**
 * Generates the CBOR-encoded version of the given Hop Count and writes it
 * into the passed buffer. The buffer must be at least be
 * BUNDLE7_HOP_COUNT_MAX_ENCODED_SIZE bytes long
 *
 * @param bundle_hop_count
 * @param buffer Output buffer
 * @param length Buffer length
 * @return Number of bytes written into buffer
 */
size_t bundle7_hop_count_serialize(const struct bundle_hop_count *hop_count,
	uint8_t *buffer, size_t length);

#endif // BUNDLE7_HOPCOUNT_H_INCLUDED
