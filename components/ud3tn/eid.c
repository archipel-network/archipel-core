#include "upcn/eid.h"
#include "upcn/result.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

enum upcn_result validate_eid(const char *eid)
{
	if (!eid)
		return UPCN_FAIL;

	const char *colon = strchr((char *)eid, ':');
	uint64_t node, service;

	if (colon == NULL)
		return UPCN_FAIL; // EID has to contain a scheme:ssp separator
	if (colon - eid != 3)
		return UPCN_FAIL; // unknown scheme
	if (!memcmp(eid, "dtn", 3) && strlen(colon + 1) != 0)
		return UPCN_OK; // proper "dtn:" EID
	if (!memcmp(eid, "ipn", 3)) {
		// check "ipn:node.service" EID
		if (sscanf(eid, "ipn:%"PRIu64".%"PRIu64, &node, &service) == 2)
			return UPCN_OK;
	}
	// unknown scheme
	return UPCN_FAIL;
}
