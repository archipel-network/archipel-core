#include "ud3tn/common.h"
#include "ud3tn/eid.h"
#include "ud3tn/result.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// https://datatracker.ietf.org/doc/html/draft-ietf-dtn-bpbis-31#section-4.2.5.1
enum ud3tn_result validate_eid(const char *const eid)
{
	size_t len;
	const char *cur;

	switch (get_eid_scheme(eid)) {
	case EID_SCHEME_DTN:
		len = strlen(eid);
		// minimum is dtn:none or dtn://?/ with C being a char
		if (len < 8)
			return UD3TN_FAIL;
		else if (len == 8 && memcmp(eid, "dtn:none", 8) == 0)
			return UD3TN_OK;
		else if (memcmp(eid, "dtn://", 6))
			return UD3TN_FAIL;
		// check node-name
		cur = &eid[6]; // after "dtn://"
		while (*cur) {
			if (!isalnum(*cur) && *cur != '-' &&
			    *cur != '.' && *cur != '_')
				break;
			cur++;
		}
		// fail if node-name is zero-length or not followed by '/'
		if (cur == &eid[6] || *cur != '/')
			return UD3TN_FAIL;
		// check demux
		return validate_dtn_eid_demux(cur);
	case EID_SCHEME_IPN:
		return validate_ipn_eid(eid, NULL, NULL);
	default:
		return UD3TN_FAIL;
	}
}

enum ud3tn_result validate_dtn_eid_demux(const char *demux)
{
	if (!demux)
		return UD3TN_FAIL;

	while (*demux) {
		// VCHAR is in range %x21-7E -> RFC 5234)
		if (*demux < 0x21 || *demux > 0x7E)
			return UD3TN_FAIL;
		demux++;
	}
	return UD3TN_OK;
}

enum ud3tn_result validate_local_eid(const char *const eid)
{
	if (validate_eid(eid) != UD3TN_OK)
		return UD3TN_FAIL;

	uint64_t service;

	switch (get_eid_scheme(eid)) {
	case EID_SCHEME_DTN:
		// The EID must start with dtn://
		if (strlen(eid) < 7 || memcmp(eid, "dtn://", 6))
			return UD3TN_FAIL;
		// The first-contained slash must be there and terminate the EID
		if (strchr((char *)&eid[6], '/') - strlen(eid) + 1 == eid)
			return UD3TN_OK;
		return UD3TN_FAIL;
	case EID_SCHEME_IPN:
		// Service number must be zero
		if (validate_ipn_eid(eid, NULL, &service) == UD3TN_OK &&
		    service == 0)
			return UD3TN_OK;
		return UD3TN_FAIL;
	default:
		return UD3TN_FAIL;
	}
}

enum eid_scheme get_eid_scheme(const char *const eid)
{
	if (!eid)
		return EID_SCHEME_UNKNOWN;

	const size_t len = strlen(eid);

	if (len >= 4 && !memcmp(eid, "dtn:", 4))
		return EID_SCHEME_DTN;
	else if (len >= 4 && !memcmp(eid, "ipn:", 4))
		return EID_SCHEME_IPN;

	return EID_SCHEME_UNKNOWN;
}

const char *parse_ipn_ull(const char *const cur, uint64_t *const out)
{
	unsigned long long tmp;
	const char *next, *next_strtoull;

	if (!cur || *cur == '\0')
		return NULL;

	// Check that only digits are part of the substring
	// Otherwise, strtoull may accept things like '-' and thousands
	// separators according to the currently set locale.
	next = cur;
	while (*next && *next >= '0' && *next <= '9')
		next++;
	if (*next != '.' && *next != '\0')
		return NULL;

	errno = 0;
	tmp = strtoull(cur, (char **)&next_strtoull, 10);

	// strtoull must have parsed the same as the digit check above
	if (next != next_strtoull)
		return NULL;
	// Special case: returns 0 -> failed OR is actually zero
	if (tmp == 0 && (next - cur != 1 || *cur != '0'))
		return NULL;
	// Special case: overflow
	if (tmp == ULLONG_MAX && errno == ERANGE)
		return NULL;

	if (out)
		*out = (uint64_t)tmp;

	return next;
}

// https://datatracker.ietf.org/doc/html/rfc6260
// check "ipn:node.service" EID
enum ud3tn_result validate_ipn_eid(
	const char *const eid,
	uint64_t *const node_out, uint64_t *const service_out)
{
	if (get_eid_scheme(eid) != EID_SCHEME_IPN)
		return UD3TN_FAIL;

	const char *cur;

	cur = parse_ipn_ull(&eid[4], node_out);
	if (!cur || *cur != '.')
		return UD3TN_FAIL;

	cur = parse_ipn_ull(cur + 1, service_out);
	if (!cur || *cur != '\0')
		return UD3TN_FAIL;

	return UD3TN_OK;
}

char *get_node_id(const char *const eid)
{
	if (validate_eid(eid) != UD3TN_OK)
		return NULL;

	char *delim, *result;

	switch (get_eid_scheme(eid)) {
	case EID_SCHEME_DTN:
		// Special case, e.g., "dtn:none"
		if (strlen(eid) < 7 || memcmp(eid, "dtn://", 6))
			return strdup(eid);
		delim = strchr((char *)&eid[6], '/');
		ASSERT(delim);
		// First-contained slash ends the EID (-> EID _is_ node ID)
		if (delim - strlen(eid) + 1 == eid)
			return strdup(eid);
		result = strdup(eid);
		result[delim - eid + 1] = '\0';
		return result;
	case EID_SCHEME_IPN:
		result = strdup(eid);
		delim = strchr(result, '.');
		ASSERT(delim);
		// No out-of-bounds access as validate_eid ensures the dot is
		// always followed by at least one digit.
		ASSERT(strlen(delim) >= 2);
		delim[1] = '0';
		delim[2] = '\0';
		return result;
	default:
		return NULL;
	}
}
