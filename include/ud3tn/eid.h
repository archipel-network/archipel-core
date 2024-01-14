// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef EID_H_INCLUDED
#define EID_H_INCLUDED

#include "ud3tn/result.h"

#include <limits.h>
#include <stdint.h>

#define EID_MAX_LEN (INT16_MAX - 1)

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
 * Validate the demux part of a dtn EID, i.e., the agent ID for us.
 *
 * @param demux demux string (null-terminated)
 *
 * @return UD3TN_OK if the demux string is valid.
 */
enum ud3tn_result validate_dtn_eid_demux(const char *demux);

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
 * Pre-processes local node IDs specified by users such that they have a
 * consistent internal representation, e.g., by adding a trailing slash for
 * dtn-scheme EIDs. Note that this does not replace the need for validating the
 * EID afterwards.
 *
 * @param eid EID string
 *
 * @return A copy of eid, potentially modified to make it a valid local node ID.
 */
char *preprocess_local_eid(const char *eid);

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

/**
 * Get the node ID for a given EID.
 *
 * @param eid EID string
 *
 * @return For ipn EIDs, this returns "ipn:N.0" whereas "N" is the node number.
 *         For dtn EIDs, "dtn://node/" is returned, wheras "node" is the node
 *         name. On error, NULL is returned.
 */
char *get_node_id(const char *const eid);

/**
 * Obtain a pointer to the agent ID for a given EID. Assumes that the provided
 * EID is valid.
 *
 * @param eid EID string
 *
 * @return A pointer to the start of the agent ID inside the given EID string,
 *         NULL on error.
 */
const char *get_agent_id_ptr(const char *const eid);

#endif // EID_H_INCLUDED
