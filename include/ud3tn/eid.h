#ifndef EID_H_INCLUDED
#define EID_H_INCLUDED

#include "ud3tn/result.h"

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

#endif // EID_H_INCLUDED
