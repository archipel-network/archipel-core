// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef BUNDLE7_REPORTS_INCLUDED
#define BUNDLE7_REPORTS_INCLUDED

#include "ud3tn/bundle.h"  // struct bundle, struct bundle_status_report

#include <stddef.h>
#include <stdint.h>

/**
 * Generates a BPv7-bis administrative record bundle of type
 * "Bundle status report" for the given bundle that can be send
 * to the Report-To EID.
 */
struct bundle *bundle7_generate_status_report(
	const struct bundle * const bundle,
	const struct bundle_status_report *report,
	const char *source,
	const uint64_t timestamp_ms);


/**
 * Generates a BPv7-bis administrative record bundle of type
 * "Custody Signal" for the given bundle that can be send to
 * the current custodian EID.
 *
 * @param bundle Bundle for which the custody signal should be created
 * @param signal
 * @oaram source Source EID for the custody signal bundle
 */
struct bundle_list *bundle7_generate_custody_signal(
	const struct bundle * const bundle,
	const struct bundle_custody_signal *signal,
	const char *source,
	const uint64_t timestamp_ms);

/**
 * Parses the payload block of the a BPv7-bis administrative record
 */
struct bundle_administrative_record *bundle7_parse_administrative_record(
	const uint8_t *const data, const size_t length);


void free_record_fields(struct bundle_administrative_record *record);

#endif /* BUNDLE7_REPORTS_INCLUDED */
