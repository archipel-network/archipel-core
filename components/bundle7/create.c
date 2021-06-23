#include "bundle7/bundle7.h"
#include "bundle7/create.h"

#include "ud3tn/bundle.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


struct bundle *bundle7_create_local(
	void *payload, size_t payload_length,
	const char *source, const char *destination,
	uint64_t creation_time_s, uint64_t sequence_number, uint64_t lifetime_s,
	enum bundle_proc_flags proc_flags)
{
	struct bundle *bundle = bundle_init();

	if (bundle == NULL) {
		free(payload);
		return NULL;
	}

	bundle->protocol_version = 0x7;
	bundle->proc_flags = proc_flags;

	// Creation time
	bundle->creation_timestamp_ms = creation_time_s * 1000;
	bundle->sequence_number = sequence_number;
	bundle->lifetime_ms = lifetime_s * 1000;

	bundle->crc_type = DEFAULT_CRC_TYPE;

	// Create payload block and block list
	bundle->payload_block = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	bundle->blocks = bundle_block_entry_create(bundle->payload_block);

	if (bundle->payload_block == NULL || bundle->blocks == NULL)
		goto fail;

	bundle->source = strdup(source);
	if (bundle->source == NULL)
		goto fail;

	bundle->destination = strdup(destination);
	if (bundle->destination == NULL)
		goto fail; // bundle_free takes care of source

	bundle->report_to = strdup("dtn:none");
	if (bundle->report_to == NULL)
		goto fail; // bundle_free takes care of source and destination

	bundle->payload_block->data = payload;
	bundle->payload_block->length = payload_length;

	bundle7_recalculate_primary_block_length(bundle);

	return bundle;

fail:
	free(payload);
	bundle_free(bundle);
	return NULL;
}
