#ifndef REPORTMANAGER_H_INCLUDED
#define REPORTMANAGER_H_INCLUDED

#include "ud3tn/bundle.h"

#include <stddef.h>
#include <stdint.h>


struct bundle *generate_status_report(
	const struct bundle * const bundle,
	const struct bundle_status_report *report,
	const char *local_eid);


struct bundle_list *generate_custody_signal(
	const struct bundle * const bundle,
	const struct bundle_custody_signal *signal,
	const char *local_eid);


/**
 * Parses the payload block of an administrative record bundle.
 * If case of error, NULL will be returned.
 */
struct bundle_administrative_record *parse_administrative_record(
	uint8_t protocol_version,
	const uint8_t *const data, const size_t length);


void free_administrative_record(struct bundle_administrative_record *record);

#endif /* REPORTMANAGER_H_INCLUDED */
