#ifndef BUNDLE6_REPORTS_INCLUDED
#define BUNDLE6_REPORTS_INCLUDED

#include "ud3tn/bundle.h"  // struct bundle, struct bundle_status_report

#include <stddef.h>
#include <stdint.h>

/**
 * Generates a RFC 5050 administrative record bundle of type
 * "Bundle status report" for the given bundle that can be send
 * to the Report-To EID.
 */
struct bundle *bundle6_generate_status_report(
	const struct bundle * const bundle,
	const struct bundle_status_report *report,
	const char *local_eid);


/**
 * Generates a RFC 5050 administrative record bundle of type
 * "Custody Signal" for the given bundle that can be send to
 * the current custodian EID.
 */
struct bundle *bundle6_generate_custody_signal(
	const struct bundle * const bundle,
	const struct bundle_custody_signal *signal,
	const char *local_eid);


/**
 * Parses the payload block of the a RFC 5050 administrative record
 */
struct bundle_administrative_record *bundle6_parse_administrative_record(
	const uint8_t *const data, const size_t length);


#endif /* BUNDLE6_REPORTS_INCLUDED */
