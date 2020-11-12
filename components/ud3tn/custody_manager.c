#include "ud3tn/bundle.h"
#include "ud3tn/bundle_storage_manager.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/custody_manager.h"

#include "bundle6/bundle6.h"
#include "bundle7/bundle7.h"

#include <stdlib.h>
#include <string.h>


static struct bundle *accepted_bundles[CUSTODY_MAX_BUNDLE_COUNT];
static int accepted_bundle_count;

static const char *own_eid;

static int get_index(struct bundle *bundle)
{
	int i;

	for (i = 0; i < accepted_bundle_count; i++) {
		if (accepted_bundles[i] == bundle)
			return i;
	}
	return -1;
}

static int find(uint64_t creation_timestamp_ms, uint16_t sequence_number,
	char *source_eid, uint32_t fragment_offset, uint32_t fragment_length)
{
	int i;
	struct bundle *b;

	for (i = 0; i < accepted_bundle_count; i++) {
		b = accepted_bundles[i];
		if (
			b->creation_timestamp_ms == creation_timestamp_ms
			&& b->sequence_number == sequence_number
			&& strcmp(b->source, source_eid) == 0
			&& (!bundle_is_fragmented(b)
				|| (b->fragment_offset == fragment_offset
				&& b->payload_block->length == fragment_length))
		) {
			return i;
		}
	}
	return -1;
}

bool custody_manager_has_redundant_bundle(struct bundle *bundle)
{
	return find(bundle->creation_timestamp_ms, bundle->sequence_number,
		bundle->source, bundle->fragment_offset,
		bundle->payload_block->length) != -1;
}

bool custody_manager_storage_is_acceptable(struct bundle *bundle)
{
	/*
	 * RFC 5050 states: "The conditions under which a node may accept
	 * custody of a bundle whose destination is not a singleton endpoint
	 * are not defined in this specification." Thus, we have to check
	 * in the case of Bundle Protocol v6 if the endpoint is a singleton
	 * for well-defined behavior.
	 */
	bool bpv6_se_check = 0;

	if (bundle->protocol_version == 6)
		bpv6_se_check = !HAS_FLAG(bundle->proc_flags,
			BUNDLE_V6_FLAG_SINGLETON_ENDPOINT);

	return !(
		accepted_bundle_count >= CUSTODY_MAX_BUNDLE_COUNT
		|| get_index(bundle) != -1
		|| bundle_get_serialized_size(bundle) > CUSTODY_MAX_BUNDLE_SIZE
		|| bpv6_se_check
		);
}

bool custody_manager_has_accepted(struct bundle *bundle)
{
	return get_index(bundle) != -1;
}

struct bundle *custody_manager_get_by_record(
	struct bundle_administrative_record *record)
{
	int index;

	index = find(
		record->bundle_creation_timestamp_ms,
		record->bundle_sequence_number,
		record->bundle_source_eid,
		record->fragment_offset,
		record->fragment_length
	);
	if (index == -1)
		return NULL;
	else
		return accepted_bundles[index];
}

/* 5.10.1 */
enum ud3tn_result custody_manager_accept(struct bundle *bundle)
{
	/* Should be checked by bundle processor */
	ASSERT(!custody_manager_has_redundant_bundle(bundle));
	ASSERT(custody_manager_storage_is_acceptable(bundle));
	/* Add to list */
	accepted_bundles[accepted_bundle_count++] = bundle;
	/* Add ret. constraint */
	bundle->ret_constraints |= BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED;
	/* Add own EID as custodian and to dict */

	switch (bundle->protocol_version) {
	case 6:
		free(bundle->current_custodian);
		bundle->current_custodian = strdup(own_eid);
		return UD3TN_OK;
	default:
		return UD3TN_FAIL;
	}
}

/* 5.10.2 */
void custody_manager_release(struct bundle *bundle)
{
	size_t num;
	int index = get_index(bundle);

	if (index == -1)
		return;
	if (index != (accepted_bundle_count - 1)) {
		num = accepted_bundle_count - index - 1;
		memmove(
			accepted_bundles + index,
			accepted_bundles + index + 1,
			num
		);
	}
	accepted_bundle_count--;
	bundle->ret_constraints &= ~BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED;
	if (bundle->ret_constraints == BUNDLE_RET_CONSTRAINT_NONE) {
		bundle_storage_delete(bundle->id);
		bundle_drop(bundle);
	}
}

void custody_manager_init(const char *local_eid)
{
	own_eid = local_eid;
}
