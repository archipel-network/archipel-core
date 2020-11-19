#include "ud3tn/eid.h"
#include "ud3tn/result.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

enum ud3tn_result validate_eid(const char *eid)
{
	if (!eid)
		return UD3TN_FAIL;

	const char *colon = strchr((char *)eid, ':');
	uint64_t node, service;

	if (colon == NULL)
		return UD3TN_FAIL; // EID has to contain a scheme:ssp separator
	if (colon - eid != 3)
		return UD3TN_FAIL; // unknown scheme
	if (!memcmp(eid, "dtn", 3) && strlen(colon + 1) != 0)
		return UD3TN_OK; // proper "dtn:" EID
	if (!memcmp(eid, "ipn", 3)) {
		// check "ipn:node.service" EID
		if (sscanf(eid, "ipn:%"PRIu64".%"PRIu64, &node, &service) == 2)
			return UD3TN_OK;
	}
	// unknown scheme
	return UD3TN_FAIL;
}
