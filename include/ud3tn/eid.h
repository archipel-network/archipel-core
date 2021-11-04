#ifndef EID_H_INCLUDED
#define EID_H_INCLUDED

#include "ud3tn/result.h"

#include <stdint.h>

enum eid_scheme {
	EID_SCHEME_UNKNOWN,
	EID_SCHEME_DTN,
	EID_SCHEME_IPN,
};

/**
 * Performs validation for the given EID string.
 *
 * @param eid EID string
 *
 * @return UD3TN_OK if the EID string is valid.
 */
enum ud3tn_result validate_eid(const char *eid);

/**
 * Performs validation for the given EID string and checks if it can serve
 * as a local (node) ID.
 *
 * @param eid EID string
 *
 * @return UD3TN_OK if the EID string is valid and usable as local EID.
 */
enum ud3tn_result validate_local_eid(const char *eid);

/**
 * Determine the EID scheme for the given EID string.
 *
 * @param eid EID string
 *
 * @return The EID scheme; EID_SCHEME_UNKNOWN if no scheme can be determined.
 */
enum eid_scheme get_eid_scheme(const char *eid);

/**
 * Parse (and validate) the unsigned int64 contained in an ipn EID.
 *
 * @param cur String starting with the number to be parsed
 * @param out An optional pointer to return the parsed number
 *
 * @return A pointer to the first character after the number or NULL on failure.
 */
const char *parse_ipn_ull(const char *const cur, uint64_t *const out);

/**
 * Validate the given ipn-scheme EID.
 *
 * @param eid EID string
 * @param node_out An optional pointer to return the node number
 * @param service_out An optional pointer to return the service number
 *
 * @return UD3TN_OK if the EID string is a valid ipn EID.
 */
enum ud3tn_result validate_ipn_eid(
	const char *const eid,
	uint64_t *const node_out, uint64_t *const service_out);

#endif // EID_H_INCLUDED
