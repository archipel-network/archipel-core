#include "bundle7/fragment.h"

#include "ud3tn/bundle.h"
#include "ud3tn/bundle_fragmenter.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// 5.8. Bundle Fragmentation
struct bundle *bundle7_fragment_bundle(struct bundle *working_bundle,
	uint64_t first_max)
{
	size_t first_min = bundle_get_first_fragment_min_size(
		working_bundle);

	// Cannot fragment because upper bound is too low
	if (first_min > first_max)
		return NULL;

	// Calculate the size of first fragment's payload
	uint32_t first_payload_length = first_max - first_min;

	// No need for fragmentation
	if (first_payload_length >= working_bundle->payload_block->length)
		return working_bundle;

	// Create second fragment and initialize payload
	struct bundle *remainder = bundlefragmenter_create_new_fragment(
		working_bundle, false);

	if (remainder == NULL)
		return NULL;

	// Replicate required extension blocks
	struct bundle_block_list *cur_block = working_bundle->blocks;
	struct bundle_block_list *prev = NULL;

	while (cur_block != NULL) {
		if (
			bundle_block_must_be_replicated(cur_block->data)
			&& cur_block->data->type != BUNDLE_BLOCK_TYPE_PAYLOAD
		) {
			struct bundle_block_list *entry
				= bundle_block_entry_dup(cur_block);

			// Something went south, free all replicated blocks
			if (entry == NULL) {
				bundle_free(remainder);
				return NULL;
			}

			// Link previous block to duplicate
			if (prev != NULL)
				prev->next = entry;
			// Set first block entry
			else
				remainder->blocks = entry;

			prev = entry;
		}
		cur_block = cur_block->next;
	}

	// Copy second fragment to payload block
	remainder->payload_block = bundle_block_create(
		BUNDLE_BLOCK_TYPE_PAYLOAD
	);
	if (!remainder->payload_block) {
		bundle_free(remainder);
		return NULL;
	}
	remainder->payload_block->length
		= working_bundle->payload_block->length - first_payload_length;
	remainder->payload_block->data
		= malloc(remainder->payload_block->length);
	if (remainder->payload_block->data == NULL) {
		bundle_free(remainder);
		return NULL;
	}
	memcpy(
		remainder->payload_block->data,
		working_bundle->payload_block->data + first_payload_length,
		remainder->payload_block->length
	);

	// Link last block with payload block
	struct bundle_block_list *payload_entry = bundle_block_entry_create(
		remainder->payload_block);

	if (payload_entry == NULL) {
		bundle_free(remainder);
		return NULL;
	}

	if (prev != NULL)
		prev->next = payload_entry;
	else
		remainder->blocks = payload_entry;

	// Set total ADU length if not already fragmented
	if (!bundle_is_fragmented(working_bundle))
		working_bundle->total_adu_length
			= working_bundle->payload_block->length;

	// Shorten first fragment's payload block
	working_bundle->payload_block->data
		= realloc(working_bundle->payload_block->data,
			first_payload_length);

	assert(working_bundle->payload_block->data != NULL);

	// Set correct lengths and offsets
	working_bundle->payload_block->length = first_payload_length;
	remainder->fragment_offset
		= working_bundle->fragment_offset + first_payload_length;
	remainder->total_adu_length = working_bundle->total_adu_length;

	// Set fragment flag
	working_bundle->proc_flags |= BUNDLE_FLAG_IS_FRAGMENT;
	remainder->proc_flags |= BUNDLE_FLAG_IS_FRAGMENT;

	// Recalculate primary block sizes
	if (bundle_recalculate_header_length(working_bundle) == UD3TN_FAIL
			|| bundle_recalculate_header_length(remainder)
				== UD3TN_FAIL)
		return NULL;

	return remainder;
}
