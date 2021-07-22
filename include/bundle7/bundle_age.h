#ifndef BUNDLE_AGE_H_INCLUDED
#define BUNDLE_AGE_H_INCLUDED

#include "ud3tn/bundle.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/**
 * Parses the given CBOR-encoded data into a uint64_t value.
 * This function can be used to parse the payload of a bundle age
 * extension block.
 *
 * @param bundle_age Pointer for returning the result
 * @param buffer CBOR-encoded data
 * @param length CBOR data length
 */
bool bundle_age_parse(uint64_t *bundle_age, const uint8_t *buffer,
	size_t length);


/**
 * Maximum length of a CBOR-encoded bundle age
 * - 5 Bit length information
 * - 8 Byte uint64_t value
 */
#define BUNDLE_AGE_MAX_ENCODED_SIZE 9


/**
 * Generates the CBOR-encoded version of the given bundle age in milliseconds
 * and writes it into the passed buffer. The buffer must be at least be
 * BUNDLE_AGE_MAX_ENCODED_SIZE bytes long
 *
 * @param bundle_age uint_64_t Bundle Age in milliseconds
 * @param buffer Output buffer
 * @param length Buffer length
 * @return Number of bytes written into buffer
 */
size_t bundle_age_serialize(const uint64_t bundle_age, uint8_t *const buffer,
	const size_t length);

#endif // BUNDLE_AGE_H_INCLUDED
