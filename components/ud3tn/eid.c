#include "ud3tn/eid.h"
#include "ud3tn/result.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum ud3tn_result validate_eid(const char *const eid)
{
	if (!eid)
		return UD3TN_FAIL;

	const char *colon = strchr((char *)eid, ':');
	uint64_t node, service;
	int len;

	if (colon == NULL)
		return UD3TN_FAIL; // EID has to contain a scheme:ssp separator
	if (colon - eid != 3)
		return UD3TN_FAIL; // unknown scheme

	// https://www.mitre.org/sites/default/files/pdf/09_5229.pdf
	if (!memcmp(eid, "dtn", 3) && strlen(colon + 1) != 0)
		return UD3TN_OK; // proper "dtn:" EID

	// https://datatracker.ietf.org/doc/html/rfc6260
	if (!memcmp(eid, "ipn", 3)) {
		// check "ipn:node.service" EID
		if (sscanf(eid, "ipn:%"PRIu64".%"PRIu64"%n",
			   &node, &service, &len) != 2)
			return UD3TN_FAIL;
		if (strlen(eid) == (size_t)len)
			return UD3TN_OK;
	}

	// unknown scheme
	return UD3TN_FAIL;
}

enum ud3tn_result validate_local_eid(const char *const eid)
{
	if (validate_eid(eid) != UD3TN_OK)
		return UD3TN_FAIL;

	const size_t len = strlen(eid);
	uint64_t node, service;

	if (!memcmp(eid, "dtn", 3)) {
		// Must start with dtn://
		if (len < 7 || memcmp(eid, "dtn://", 6))
			return UD3TN_FAIL;
		// There must be no slash after dtn://
		if (strchr((char *)&eid[6], '/'))
			return UD3TN_FAIL;
		return UD3TN_OK;
	}

	if (!memcmp(eid, "ipn", 3)) {
		// Service number must be zero
		if (sscanf(eid, "ipn:%"PRIu64".%"PRIu64, &node, &service) == 2
		    && service == 0)
			return UD3TN_OK;
	}

	return UD3TN_FAIL;
}
