#include "bundle6/fragment.h"

#include "ud3tn/bundle.h"
#include "ud3tn/common.h"
#include "ud3tn/node.h"
#include "ud3tn/bundle_fragmenter.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static enum ud3tn_result replicate_blocks(
	struct bundle *first, struct bundle *second);


struct bundle *bundle6_fragment_bundle(
	struct bundle *working_bundle, uint64_t first_max)
{
	struct bundle *remainder;
	struct bundle_block_list *cur_block;
	uint32_t first_payload_length;


	/* Determine length of first fragment's payload */
	first_payload_length = first_max
		- bundle_get_first_fragment_min_size(working_bundle);
	/* Don't fragment? */
	if (first_payload_length >= working_bundle->payload_block->length)
		return working_bundle;
	/* Construct second fragment */
	remainder = bundlefragmenter_create_new_fragment(working_bundle, true);
	if (remainder == NULL)
		return NULL;
	/* Set total ADU length if not already fragmented */
	if (!bundle_is_fragmented(working_bundle))
		working_bundle->total_adu_length
			= working_bundle->payload_block->length;
	/* Set the payload block's properties */
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
	/* Find PL block position in working bundle */
	/* Add following blocks to remainder */
	cur_block = working_bundle->blocks;
	while (cur_block != NULL) {
		if (cur_block->data == working_bundle->payload_block) {
			/* PL block of first fragment is now last block */
			cur_block->data->flags |=
				BUNDLE_V6_BLOCK_FLAG_LAST_BLOCK;
			remainder->blocks->next = cur_block->next;
			cur_block->next = NULL;
			break;
		}
		cur_block = cur_block->next;
	}
	/* Shorten first fragment's PL block */
	working_bundle->payload_block->data
		= realloc(working_bundle->payload_block->data,
			first_payload_length);
	ASSERT(working_bundle->payload_block->data != NULL);
	/* Set correct lengths and offsets */
	working_bundle->payload_block->length = first_payload_length;
	remainder->fragment_offset
		= working_bundle->fragment_offset + first_payload_length;
	/* Handle blocks that must be replicated in every fragment */
	replicate_blocks(working_bundle, remainder);
	/* Set fragment flag */
	working_bundle->proc_flags |= BUNDLE_FLAG_IS_FRAGMENT;
	/* Recalc sizes */
	if (bundle_recalculate_header_length(working_bundle) == UD3TN_FAIL ||
			bundle_recalculate_header_length(remainder) ==
			UD3TN_FAIL)
		return NULL;
	/* Return created fragment */
	return remainder;
}

/* first and second have to be split @ payload block */
/* payload block has to be last block of first bundle and first of second */
static enum ud3tn_result replicate_blocks(
	struct bundle *first, struct bundle *second)
{
	struct bundle_block_list *list = NULL;
	struct bundle_block_list **item;
	struct bundle_block_list *entry;

	/* 1) Handle replicated blocks _BEFORE_ payload */
	list = NULL;
	item = &list;
	entry = first->blocks;
	while (entry != NULL) {
		if (
			bundle_block_must_be_replicated(entry->data)
			&& entry->data->type != BUNDLE_BLOCK_TYPE_PAYLOAD
		) {
			*item = bundle_block_entry_dup(entry);
			if (*item == NULL) {
				while (list != NULL)
					list = bundle_block_entry_free(list);
				return UD3TN_FAIL;
			}
			item = &(*item)->next;
		}
		entry = entry->next;
	}
	/* Set first entry as PL block of 2nd fragment before we change it */
	entry = second->blocks; /* PL block is first till now */
	/* Prepend block list of 2nd fragment with replicated blocks */
	/* _BEFORE_ payload of 1st fragment */
	if (list != NULL) {
		(*item)->next = second->blocks;
		second->blocks = list;
	}
	/* 2) Handle replicated blocks _AFTER_ payload */
	list = NULL;
	item = &list;
	while (entry != NULL) {
		if (
			bundle_block_must_be_replicated(entry->data)
			&& entry->data->type != BUNDLE_BLOCK_TYPE_PAYLOAD
		) {
			*item = bundle_block_entry_dup(entry);
			if (*item == NULL) {
				while (list != NULL)
					list = bundle_block_entry_free(list);
				return UD3TN_FAIL;
			}
			item = &(*item)->next;
		}
		entry = entry->next;
	}
	/* Append replicated blocks _AFTER_ payload to list of first fragment */
	if (list != NULL) {
		/* Go to 1st fragment's last block (payload block) */
		entry = first->blocks;
		ASSERT(entry != NULL);
		while (entry->next != NULL)
			entry = entry->next;
		/* Remove "last_block" flag */
		entry->data->flags &= ~BUNDLE_V6_BLOCK_FLAG_LAST_BLOCK;
		/* Append */
		entry->next = list;
	}
	return UD3TN_OK;
}
