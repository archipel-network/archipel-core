#ifndef BUNDLE7_EID_H_INCLUDED
#define BUNDLE7_EID_H_INCLUDED

#include "cbor.h"

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t


// -------------------------------------
// BPv7 Endpoint Identifier (EID) Parser
// -------------------------------------

/**
 * Parses the given CBOR buffer as EID string. If the buffer is larger than
 * required the remaining bytes will be ignored.
 *
 * Internally this function uses "bundle7_eid_parse_cbor()"
 *
 * @param buffer CBOR encoded EID
 * @param length buffer size
 *
 * @return EID string. If any error occures, NULL will be returned
 */
char *bundle7_eid_parse(const uint8_t *buffer, size_t length);


/**
 * Parses EID with a given CBOR iterator
 *
 * @param it iterator to CBOR data stream / buffer
 * @param eid Destination EID string
 *
 * @return CBOR error if something went south
 */
CborError bundle7_eid_parse_cbor(CborValue *it, char **eid);


// -----------------------------------------
// BPv7 Endpoint Identifier (EID) Serializer
// -----------------------------------------

/**
 * Returns an upper bound for the maximal length of the CBOR encoded
 * version of the EID. This size can be used for buffer initializations if an
 * EID needs to be serialized.
 */
size_t bundle7_eid_get_max_serialized_size(const char *eid);


/**
 * Creates a CBOR encoded EID from given string representation.
 * If any error occure NULL will be returned.
 *
 * @param eid EID string
 * @param length Final CBOR length
 *
 * @return CBOR encoded bytes
 */
uint8_t *bundle7_eid_serialize_alloc(const char *eid, size_t *length);


/**
 * Creates CBOR encoded EID in the given output buffer. If the buffer is not
 * large enough 0 will be returned, in case of error -1 and otherwise the
 * number of bytes written to the buffer.
 *
 * Internally this function uses "bundle7_eid_serialize_cbor()"
 *
 * @param eid EID string
 * @param buffer CBOR output buffer
 * @param buffer_size length of the output buffer
 *
 * @return Number of CBOR bytes written to buffer. If an error occures -1
 */
int bundle7_eid_serialize(const char *eid, uint8_t *buffer,
	size_t buffer_size);


/**
 * Creates a CBOR encoded EID with the given CBOR encoder. You can use
 * this function to include EIDs in your CBOR encoding process.
 *
 * @param eid EID that should be encoded in CBOR
 * @param encoder CBOR encoder to already existing buffer
 *
 * @return CBOR error if something went wrong
 */
CborError bundle7_eid_serialize_cbor(const char *eid, CborEncoder *encoder);

#endif // BUNDLE7_EID_H_INCLUDED
