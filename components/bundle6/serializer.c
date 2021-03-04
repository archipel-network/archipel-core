#include "bundle6/sdnv.h"
#include "bundle6/bundle6.h"
#include "bundle6/serializer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define write_bytes(bytes, data) \
	write(cla_obj, data, bytes)
#define serialize_u16(buffer, value) \
	write_bytes(sdnv_write_u16(buffer, value), buffer)
#define serialize_u32(buffer, value) \
	write_bytes(sdnv_write_u32(buffer, value), buffer)
#define serialize_u64(buffer, value) \
	write_bytes(sdnv_write_u64(buffer, value), buffer)

struct eid_reference {
	uint16_t scheme_offset;
	uint16_t ssp_offset;
};

enum ud3tn_result bundle6_serialize(
	struct bundle *bundle,
	void (*write)(void *cla_obj, const void *, const size_t),
	void *cla_obj)
{
	uint8_t buffer[MAX_SDNV_SIZE];

	// Serialize dict
	struct bundle6_dict_descriptor *dict_desc = bundle6_calculate_dict(
		bundle
	);
	char *dict = malloc(dict_desc->dict_length_bytes);

	if (dict == NULL)
		return UD3TN_FAIL;

	bundle6_serialize_dictionary(dict, dict_desc);

	/* Write version field */
	write_bytes(1, &(bundle->protocol_version));
	/* Write SDNV values up to dictionary length */
	serialize_u32(buffer, bundle->proc_flags & (
		/* Only RFC 5050 flags */
		BUNDLE_FLAG_IS_FRAGMENT
		| BUNDLE_FLAG_ADMINISTRATIVE_RECORD
		| BUNDLE_FLAG_MUST_NOT_BE_FRAGMENTED
		| BUNDLE_V6_FLAG_CUSTODY_TRANSFER_REQUESTED
		| BUNDLE_V6_FLAG_SINGLETON_ENDPOINT
		| BUNDLE_FLAG_ACKNOWLEDGEMENT_REQUESTED
		| BUNDLE_V6_FLAG_NORMAL_PRIORITY
		| BUNDLE_V6_FLAG_EXPEDITED_PRIORITY
		| BUNDLE_FLAG_REPORT_RECEPTION
		| BUNDLE_V6_FLAG_REPORT_CUSTODY_ACCEPTANCE
		| BUNDLE_FLAG_REPORT_FORWARDING
		| BUNDLE_FLAG_REPORT_DELIVERY
		| BUNDLE_FLAG_REPORT_DELETION
	));
	serialize_u32(buffer, bundle->primary_block_length);
	serialize_u16(buffer,
		      dict_desc->destination_eid_info.dict_scheme_offset);
	serialize_u16(buffer,
		      dict_desc->destination_eid_info.dict_ssp_offset);
	serialize_u16(buffer,
		      dict_desc->source_eid_info.dict_scheme_offset);
	serialize_u16(buffer,
		      dict_desc->source_eid_info.dict_ssp_offset);
	serialize_u16(buffer,
		      dict_desc->report_to_eid_info.dict_scheme_offset);
	serialize_u16(buffer,
		      dict_desc->report_to_eid_info.dict_ssp_offset);
	serialize_u16(buffer,
		      dict_desc->custodian_eid_info.dict_scheme_offset);
	serialize_u16(buffer,
		      dict_desc->custodian_eid_info.dict_ssp_offset);
	serialize_u64(buffer, bundle->creation_timestamp_ms / 1000); // ms -> s
	serialize_u64(buffer, bundle->sequence_number);
	serialize_u64(buffer, bundle->lifetime_ms / 1000); // ms -> s
	serialize_u16(buffer, dict_desc->dict_length_bytes);
	/* Write dictionary byte array */
	write_bytes(dict_desc->dict_length_bytes, dict);
	/* Write remaining SDNV values */
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_IS_FRAGMENT)) {
		serialize_u32(buffer, bundle->fragment_offset);
		serialize_u32(buffer, bundle->total_adu_length);
	}

	/* Serialize bundle blocks */
	struct bundle_block_list *cur_entry = bundle->blocks;
	struct endpoint_list *cur_ref;
	int eid_idx = 0;

	while (cur_entry != NULL) {
		write_bytes(1, &cur_entry->data->type);
		serialize_u32(buffer, cur_entry->data->flags);
		if (HAS_FLAG(cur_entry->data->flags,
			BUNDLE_V6_BLOCK_FLAG_HAS_EID_REF_FIELD)
		) {
			// Determine the count of refs
			int eid_ref_cnt = 0;

			for (cur_ref = cur_entry->data->eid_refs; cur_ref;
			     cur_ref = cur_ref->next, eid_ref_cnt++)
				;
			serialize_u16(buffer, eid_ref_cnt);
			// Write out the refs
			for (int c = 0; c < eid_ref_cnt; c++, eid_idx++) {
				struct bundle6_eid_info eid_info =
					dict_desc->eid_references[c];
				serialize_u16(buffer,
					      eid_info.dict_scheme_offset);
				serialize_u16(buffer,
					      eid_info.dict_ssp_offset);
			}
		}
		serialize_u32(buffer, cur_entry->data->length);
		write_bytes(cur_entry->data->length, cur_entry->data->data);
		cur_entry = cur_entry->next;
	}

	free(dict);
	free(dict_desc);

	return UD3TN_OK;
}
