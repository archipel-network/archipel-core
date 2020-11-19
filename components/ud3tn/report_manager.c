#include "ud3tn/bundle.h"
#include "ud3tn/parser.h"
#include "ud3tn/report_manager.h"

#include "bundle6/reports.h"
#include "bundle7/reports.h"

#include <stdlib.h>
#include <stdint.h>


struct bundle *generate_status_report(
	const struct bundle * const bundle,
	const struct bundle_status_report *report,
	const char *local_eid)
{
	switch (bundle->protocol_version) {
	// RFC 5050
	case 6:
		return bundle6_generate_status_report(
			bundle,
			report,
			local_eid
		);
	// BPv7-bis
	case 7:
		return bundle7_generate_status_report(
			bundle,
			report,
			local_eid
		);
	default:
		return NULL;
	}
}


struct bundle_list *generate_custody_signal(
	const struct bundle * const bundle,
	const struct bundle_custody_signal *signal,
	const char *local_eid)
{
	struct bundle *signal_bundle;

	switch (bundle->protocol_version) {
	// RFC 5050
	case 6:
		signal_bundle = bundle6_generate_custody_signal(
				bundle,
				signal,
				local_eid
		);
		return bundle_list_entry_create(signal_bundle);
	default:
		return NULL;
	}
}


struct bundle_administrative_record *parse_administrative_record(
	uint8_t protocol_version,
	const uint8_t *const data, const size_t length)
{
	switch (protocol_version) {
	// RFC 5050
	case 6:
		return bundle6_parse_administrative_record(data, length);
	// BPv7-bis
	case 7:
		return bundle7_parse_administrative_record(data, length);
	default:
		return NULL;
	}
}


void free_administrative_record(struct bundle_administrative_record *record)
{
	if (record != NULL) {
		free(record->custody_signal);
		free(record->status_report);
		free(record->bundle_source_eid);
		free(record);
	}
}
