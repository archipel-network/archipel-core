#include "ud3tn/bundle.h"
#include "ud3tn/bundle_fragmenter.h"
#include "ud3tn/common.h"

#include "bundle6/fragment.h"
#include "bundle7/fragment.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


struct bundle *bundlefragmenter_create_new_fragment(
	struct bundle const *prototype, bool init_payload)
{
	struct bundle *fragment;
	struct bundle_block *fragment_pl;

	fragment = malloc(sizeof(struct bundle));
	if (fragment == NULL)
		return NULL;
	bundle_copy_headers(fragment, prototype);
	/* Reset the ID */
	fragment->id = BUNDLE_INVALID_ID;
	/* Set total ADU length if not already fragmented */
	if (!bundle_is_fragmented(prototype))
		fragment->total_adu_length = prototype->payload_block->length;
	else
		fragment->total_adu_length = prototype->total_adu_length;
	/* Set fragment flag */
	fragment->proc_flags |= BUNDLE_FLAG_IS_FRAGMENT;
	if (init_payload) {
		/* Create new PL block */
		fragment_pl = bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
		if (fragment_pl == NULL) {
			free(fragment);
			return NULL;
		}
		fragment_pl->flags = prototype->payload_block->flags;
		/* Insert the payload block */
		fragment->blocks = bundle_block_entry_create(fragment_pl);
		if (fragment->blocks == NULL) {
			bundle_block_free(fragment_pl);
			free(fragment);
			return NULL;
		}
		/* Set the bundle's payload reference */
		fragment->payload_block = fragment_pl;
	} else {
		fragment->blocks = NULL;
		fragment->payload_block = NULL;
	}
	return fragment;
}


struct bundle *bundlefragmenter_initialize_first_fragment(struct bundle *input)
{
	struct bundle *result;
	struct bundle_block_list *cur_block;

	ASSERT(input != NULL);
	result = bundlefragmenter_create_new_fragment(input, false);
	if (result == NULL)
		return NULL;

	// Copy all extension blocks (includung payload block) to fragment
	cur_block = bundle_block_list_dup(input->blocks);
	result->blocks = cur_block;

	// Set payload block reference
	while (cur_block != NULL) {
		if (cur_block->data->type == BUNDLE_BLOCK_TYPE_PAYLOAD) {
			result->payload_block = cur_block->data;
			break;
		}
		cur_block = cur_block->next;
	}

	// No payload block was found
	if (result->payload_block == NULL) {
		bundle_free(result);
		return NULL;
	}

	// Calculate correct primary block length
	if (bundle_recalculate_header_length(result) == UD3TN_FAIL)
		return NULL;
	return result;
}


struct bundle *bundlefragmenter_fragment_bundle(struct bundle *working_bundle,
	uint64_t first_max)
{
	switch (working_bundle->protocol_version) {
	// RFC 5050
	case 6:
		return bundle6_fragment_bundle(working_bundle, first_max);
	// BPv7-bis
	case 7:
		return bundle7_fragment_bundle(working_bundle, first_max);
	default:
		return NULL;
	}
}
