#include "bundle6/bundle6.h"
#include "bundle6/sdnv.h"

#include "ud3tn/bundle.h"
#include "ud3tn/common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


static size_t write_scheme(char *dst, const char *eid)
{
	if (eid == NULL) {
		memcpy(dst, "dtn", 4);
		return 4;
	}

	const char *colon = strchr(eid, ':');

	if (colon == NULL) {
		memcpy(dst, "dtn", 4);
		return 4;
	}

	const size_t scheme_len = colon - eid;

	memcpy(dst, eid, scheme_len);
	dst[scheme_len] = 0;
	return scheme_len + 1;
}

static size_t write_ssp(char *dst, const char *eid)
{
	if (eid == NULL) {
		memcpy(dst, "none", 5);
		return 5;
	}

	const char *colon = strchr(eid, ':');

	if (colon == NULL) {
		memcpy(dst, "none", 5);
		return 5;
	}

	const size_t scheme_len = colon - eid;
	const char *ssp = &eid[scheme_len + 1];
	const size_t ssp_len = strlen(ssp);

	memcpy(dst, ssp, ssp_len + 1);
	return ssp_len + 1;
}

static size_t write_eid(char *dst, const struct bundle6_eid_info info)
{
	const size_t scheme_len = write_scheme(dst, info.str_ptr);

	return scheme_len + write_ssp(&dst[scheme_len], info.str_ptr);
}

void bundle6_serialize_dictionary(char *buf,
				  const struct bundle6_dict_descriptor *desc)
{
	buf += write_eid(buf, desc->destination_eid_info);
	buf += write_eid(buf, desc->source_eid_info);
	buf += write_eid(buf, desc->report_to_eid_info);
	buf += write_eid(buf, desc->custodian_eid_info);

	for (size_t i = 0; i < desc->eid_reference_count; i++)
		buf += write_eid(buf, desc->eid_references[i]);
}


// ----------------------
// Bundle and block sizes
// ----------------------

// Just get the length of the dict in bytes (more efficient)
size_t bundle6_get_dict_length(struct bundle *bundle)
{
	char *const basic_eids[] = {
		bundle->destination,
		bundle->source,
		bundle->report_to,
		bundle->current_custodian,
	};
	size_t dict_length = 0;

	// For every EID, its length (including the colon) plus 1 byte
	// is required: The colon is removed and two \0 chars are added.
	for (size_t i = 0; i < ARRAY_LENGTH(basic_eids); i++) {
		if (basic_eids[i])
			dict_length += strlen(basic_eids[i]) + 1;
		else
			dict_length += 9; // "dtn:none" with two terminators
	}

	struct bundle_block_list *cur_block = bundle->blocks;

	while (cur_block) {
		struct endpoint_list *cur_eid = cur_block->data->eid_refs;

		while (cur_eid) {
			dict_length += strlen(cur_eid->eid) + 1;
			cur_eid = cur_eid->next;
		}
		cur_block = cur_block->next;
	}

	return dict_length;
}

struct eid_len_info {
	size_t scheme_len;
	size_t ssp_len;
};

static struct eid_len_info analyze_eid(const char *eid)
{
	// dtn:none
	struct eid_len_info result = {
		.scheme_len = 3,
		.ssp_len = 4,
	};

	if (eid == NULL)
		return result;

	const char *colon = strchr(eid, ':');

	if (colon == NULL)
		return result;

	result.scheme_len = colon - eid;

	const char *ssp = &eid[result.scheme_len + 1];

	result.ssp_len = strlen(ssp);

	return result;
}

static struct bundle6_eid_info calculate_eid_info(
	const char *eid, size_t *dict_length_bytes)
{
	struct bundle6_eid_info result;

	struct eid_len_info eid_info = analyze_eid(eid);

	// For every EID, its length (including the colon) plus 1 byte
	// is required: The colon is removed and two \0 chars are added.
	result.str_ptr = eid;
	// Behind the colon
	result.ssp_ptr = eid + eid_info.scheme_len + 1;
	// Start of scheme (index) is at current dict length value
	result.dict_scheme_offset = *dict_length_bytes;
	// SSP starts after zero-termination of scheme
	result.dict_ssp_offset = *dict_length_bytes + eid_info.scheme_len + 1;
	// Extend length as last operation (used above!)
	*dict_length_bytes += eid_info.scheme_len + eid_info.ssp_len + 2;

	return result;
}

// Calculate all offsets and lengths within the dict
// NOTE: This assumes that the bundle is not modified while the resulting
// structure should be used!
struct bundle6_dict_descriptor *bundle6_calculate_dict(struct bundle *bundle)
{
	size_t eid_reference_count;
	struct bundle_block_list *cur_block;
	struct endpoint_list *cur_eid;

	// Calculate total amount of EID refs in extension blocks
	eid_reference_count = 0;
	cur_block = bundle->blocks;
	while (cur_block) {
		if (HAS_FLAG(cur_block->data->flags,
			     BUNDLE_V6_BLOCK_FLAG_HAS_EID_REF_FIELD)) {
			cur_eid = cur_block->data->eid_refs;
			while (cur_eid) {
				eid_reference_count++;
				cur_eid = cur_eid->next;
			}
		}
		cur_block = cur_block->next;
	}

	// Allocate memory for desciptor
	struct bundle6_dict_descriptor *result = malloc(
		sizeof(struct bundle6_dict_descriptor) +
		sizeof(struct bundle6_eid_info) * eid_reference_count
	);

	result->eid_reference_count = eid_reference_count;
	result->dict_length_bytes = 0;
	result->destination_eid_info = calculate_eid_info(
		bundle->destination, &result->dict_length_bytes
	);
	result->source_eid_info = calculate_eid_info(
		bundle->source, &result->dict_length_bytes
	);
	result->report_to_eid_info = calculate_eid_info(
		bundle->report_to, &result->dict_length_bytes
	);
	result->custodian_eid_info = calculate_eid_info(
		bundle->current_custodian, &result->dict_length_bytes
	);

	// Add list of EID references
	int i = 0;

	cur_block = bundle->blocks;
	while (cur_block) {
		cur_eid = cur_block->data->eid_refs;
		while (cur_eid) {
			result->eid_references[i++] = calculate_eid_info(
				cur_eid->eid, &result->dict_length_bytes
			);
			cur_eid = cur_eid->next;
		}
		cur_block = cur_block->next;
	}

	return result;
}

struct bundle6_dict_descriptor *bundle6_recalculate_header_length_internal(
	struct bundle *bundle)
{
	struct bundle6_dict_descriptor *ddesc = bundle6_calculate_dict(bundle);

	bundle->primary_block_length
		= 1
		+ sdnv_get_size_u32(bundle->proc_flags)
		+ sdnv_get_size_u32(bundle->primary_block_length)
		+ sdnv_get_size_u32(
			ddesc->destination_eid_info.dict_scheme_offset
		)
		+ sdnv_get_size_u32(
			ddesc->destination_eid_info.dict_ssp_offset
		)
		+ sdnv_get_size_u32(
			ddesc->source_eid_info.dict_scheme_offset
		)
		+ sdnv_get_size_u32(
			ddesc->source_eid_info.dict_ssp_offset
		)
		+ sdnv_get_size_u32(
			ddesc->report_to_eid_info.dict_scheme_offset
		)
		+ sdnv_get_size_u32(
			ddesc->report_to_eid_info.dict_ssp_offset
		)
		+ sdnv_get_size_u32(
			ddesc->custodian_eid_info.dict_scheme_offset
		)
		+ sdnv_get_size_u32(
			ddesc->custodian_eid_info.dict_ssp_offset
		)
		+ sdnv_get_size_u64(bundle->creation_timestamp_ms / 1000) // ms->s
		+ sdnv_get_size_u64(bundle->sequence_number)
		+ sdnv_get_size_u64(bundle->lifetime_ms / 1000) // ms->s
		+ sdnv_get_size_u32(ddesc->dict_length_bytes)
		+ ddesc->dict_length_bytes
		+ (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_IS_FRAGMENT)
			? (sdnv_get_size_u32(bundle->fragment_offset)
				+ sdnv_get_size_u32(bundle->total_adu_length))
			: 0);

	return ddesc;
}

void bundle6_recalculate_header_length(struct bundle *bundle)
{
	free(bundle6_recalculate_header_length_internal(bundle));
}

static size_t get_serialized_size(struct bundle *bundle, bool exclude_payload,
				  bool first_fragment, bool last_fragment)
{
	struct bundle6_dict_descriptor *ddesc =
		bundle6_recalculate_header_length_internal(bundle);
	size_t result = bundle->primary_block_length;
	const struct bundle_block_list *cur = bundle->blocks;
	bool payload_reached = false;
	int eid_ref_offset = 0;

	while (cur != NULL) {
		const struct bundle_block *block = cur->data;
		bool is_payload = block->type == BUNDLE_BLOCK_TYPE_PAYLOAD;
		bool block_wanted = (
			// Either the block is part of all fragments
			HAS_FLAG(block->flags,
				 BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED) ||
			// or we are before PL _and_ want the size of preceding
			(first_fragment && !payload_reached) ||
			// or we are after PL _and_ want the size of following.
			(last_fragment && payload_reached)
		);

		if (is_payload)
			payload_reached = true;

		size_t eid_ref_size = 0;
		size_t eid_ref_count = 0;

		// Has to be done for all blocks to count them for the index
		if (HAS_FLAG(block->flags,
			     BUNDLE_V6_BLOCK_FLAG_HAS_EID_REF_FIELD)) {
			const struct endpoint_list *cur_eid_ref =
				block->eid_refs;
			while (cur_eid_ref) {
				const struct bundle6_eid_info eid_info =
					ddesc->eid_references[eid_ref_offset];

				eid_ref_size += sdnv_get_size_u16(
					eid_info.dict_scheme_offset
				);
				eid_ref_size += sdnv_get_size_u16(
					eid_info.dict_ssp_offset
				);

				eid_ref_offset++;
				eid_ref_count++;
				cur_eid_ref = cur_eid_ref->next;
			}
			eid_ref_size += sdnv_get_size_u16(eid_ref_count);
		}

		if (block_wanted) {
			size_t block_size = (
				1
				+ sdnv_get_size_u32(block->flags)
				+ sdnv_get_size_u32(block->length)
				+ eid_ref_size
			);

			if (!(is_payload && exclude_payload))
				block_size += block->length;

			result += block_size;
		}

		cur = cur->next;
	}

	// If fragment, add sizes of the additional headers
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_IS_FRAGMENT) &&
			bundle->payload_block != NULL) {
		result += sdnv_get_size_u32(bundle->total_adu_length);
		result += sdnv_get_size_u32(bundle->fragment_offset);
	}

	free(ddesc);
	return result;
}


size_t bundle6_get_serialized_size(struct bundle *bundle)
{
	return get_serialized_size(bundle, false, true, true);
}


// -------------------
// Fragmentation sizes
// -------------------

size_t bundle6_get_first_fragment_min_size(struct bundle *bundle)
{
	return get_serialized_size(bundle, true, true, false);
}


size_t bundle6_get_mid_fragment_min_size(struct bundle *bundle)
{
	return get_serialized_size(bundle, true, false, false);
}


size_t bundle6_get_last_fragment_min_size(struct bundle *bundle)
{
	return get_serialized_size(bundle, true, false, true);
}
