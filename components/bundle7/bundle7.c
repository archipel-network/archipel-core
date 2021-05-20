#include "bundle7/bundle7.h"
#include "bundle7/eid.h"

#include "ud3tn/common.h"
#include "ud3tn/bundle.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>


const uint32_t BUNDLE_V7_FLAG_MASK = BUNDLE_FLAG_IS_FRAGMENT
	| BUNDLE_FLAG_ADMINISTRATIVE_RECORD
	| BUNDLE_FLAG_MUST_NOT_BE_FRAGMENTED
	| BUNDLE_FLAG_ACKNOWLEDGEMENT_REQUESTED
	| BUNDLE_FLAG_REPORT_STATUS_TIME
	| BUNDLE_FLAG_REPORT_RECEPTION
	| BUNDLE_FLAG_REPORT_FORWARDING
	| BUNDLE_FLAG_REPORT_DELIVERY
	| BUNDLE_FLAG_REPORT_DELETION;


size_t bundle7_cbor_uint_sizeof(uint64_t num)
{
	// Binary search
	if (num <= UINT16_MAX) {
		if (num <= UINT8_MAX)
			// Embedded unsigned integer
			if (num <= 23)
				return 1;
			// uint8
			else
				return 2;
		// uint16
		else
			return 3;
	// uint32
	} else if (num <= UINT32_MAX) {
		return 5;
	// uint64
	} else {
		return 9;
	}
}


size_t bundle7_eid_sizeof(const char *eid)
{
	// dtn:none -> [1,0] -> 0x82 0x01 0x00
	if (eid == NULL || strcmp(eid, "dtn:none") == 0)
		return 3;

	// dtn:
	if (eid[0] == 'd') {
		// length without "dtn:" prefix
		size_t length = strlen(eid) - 4;

		return 1 // CBOR array header
			+ bundle7_cbor_uint_sizeof(BUNDLE_V7_EID_SCHEMA_DTN)
			// String
			+ bundle7_cbor_uint_sizeof(length)
			+ length;
	}
	// ipn:
	else {
		uint32_t node, service;

		if (sscanf(eid, "ipn:%"PRIu32".%"PRIu32, &node, &service) < 2)
			// Error
			return 0;
		return 1 // CBOR array header
			+ bundle7_cbor_uint_sizeof(BUNDLE_V7_EID_SCHEMA_IPN)
			+ 1 // CBOR array header
			+ bundle7_cbor_uint_sizeof(node)
			+ bundle7_cbor_uint_sizeof(service);
	}
}


uint16_t bundle7_convert_to_protocol_block_flags(
	const struct bundle_block *block)
{
	// Convert RFC 5050 flags to BPv7-bis flags
	uint8_t flags = block->flags & (BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED
		| BUNDLE_BLOCK_FLAG_DISCARD_IF_UNPROC
		| BUNDLE_BLOCK_FLAG_REPORT_IF_UNPROC
		| BUNDLE_BLOCK_FLAG_DELETE_BUNDLE_IF_UNPROC);

	return flags;
}


void bundle7_recalculate_primary_block_length(struct bundle *bundle)
{
	// Primary Block
	size_t size = 1 // CBOR array header
		+ bundle7_cbor_uint_sizeof(bundle->protocol_version)
		+ bundle7_cbor_uint_sizeof(bundle->proc_flags)
		+ bundle7_cbor_uint_sizeof(bundle->crc_type)
		+ bundle7_eid_sizeof(bundle->destination)
		+ bundle7_eid_sizeof(bundle->source)
		+ bundle7_eid_sizeof(bundle->report_to)
		// Creation Timestamp
		+ 1  // CBOR array header
		+ bundle7_cbor_uint_sizeof(bundle->creation_timestamp_ms)
		+ bundle7_cbor_uint_sizeof(bundle->sequence_number)
		+ bundle7_cbor_uint_sizeof(bundle->lifetime_ms);

	// Fragmented Bundle
	if (bundle_is_fragmented(bundle)) {
		size += bundle7_cbor_uint_sizeof(bundle->fragment_offset);
		size += bundle7_cbor_uint_sizeof(bundle->total_adu_length);
	}

	if (bundle->crc_type == BUNDLE_CRC_TYPE_32)
		size += 5;
	else if (bundle->crc_type == BUNDLE_CRC_TYPE_16)
		size += 3;

	bundle->primary_block_length = size;
}


size_t bundle7_block_get_serialized_size(struct bundle_block *block)
{
	uint16_t flags = bundle7_convert_to_protocol_block_flags(block);
	size_t size = 1 // CBOR array header
		+ bundle7_cbor_uint_sizeof(block->type)
		+ bundle7_cbor_uint_sizeof(block->number)
		+ bundle7_cbor_uint_sizeof(flags)
		+ bundle7_cbor_uint_sizeof(block->crc_type)
		+ bundle7_cbor_uint_sizeof(block->length)
		// Block-specific data
		+ block->length;

	// CRC field
	if (block->crc_type == BUNDLE_CRC_TYPE_32)
		size += 5;
	else if (block->crc_type == BUNDLE_CRC_TYPE_16)
		size += 3;

	return size;
}


size_t bundle7_get_serialized_size(struct bundle *bundle)
{
	size_t size = 0;
	struct bundle_block_list *entry = bundle->blocks;

	// Extension Blocks
	while (entry != NULL) {
		size += bundle7_block_get_serialized_size(entry->data);
		entry = entry->next;
	}

	return 1  // CBOR indef-array start
		+ bundle->primary_block_length
		+ size
		+ 1;  // CBOR "stop"
}


size_t bundle7_get_serialized_size_without_payload(struct bundle *bundle)
{
	size_t size = 0;
	struct bundle_block_list *entry = bundle->blocks;

	// Extension Blocks
	while (entry->data->type != BUNDLE_BLOCK_TYPE_PAYLOAD) {
		size += bundle7_block_get_serialized_size(entry->data);
		entry = entry->next;
	}

	return 1  // CBOR indef-array start
		+ bundle->primary_block_length
		+ size
		+ 1;  // CBOR "stop"
}


size_t bundle7_get_first_fragment_min_size(struct bundle *bundle)
{
	size_t size = 0;
	struct bundle_block_list *entry = bundle->blocks;

	while (entry != NULL) {
		// Payload header
		if (entry->data->type == BUNDLE_BLOCK_TYPE_PAYLOAD) {
			size += 1 // CBOR array header
			+ bundle7_cbor_uint_sizeof(entry->data->type)
			+ bundle7_cbor_uint_sizeof(entry->data->number)
			+ bundle7_cbor_uint_sizeof(entry->data->flags)
			+ bundle7_cbor_uint_sizeof(entry->data->crc_type)
			+ bundle7_cbor_uint_sizeof(entry->data->length);

			// CRC field
			if (entry->data->crc_type == BUNDLE_CRC_TYPE_32)
				size += 5;
			else if (entry->data->crc_type == BUNDLE_CRC_TYPE_16)
				size += 3;

			// Data length zero + empty byte string
			size += 2;
		}
		// For the first fragment (fragment with the offset 0), all
		// extension blocks must be replicated
		else
			size += bundle7_block_get_serialized_size(entry->data);

		entry = entry->next;
	}

	// If bundle was not fragmented before, add the "Fragment offset" and
	// "Total Application Data Unit Length" field sizes
	if (!bundle_is_fragmented(bundle)) {
		size += bundle7_cbor_uint_sizeof(bundle->payload_block->length);
		// Conservative estimation for maximum "minimum bundle size",
		// i.e., header length: the fragment offset could have a maximum
		// size corresponding to the fragment offset. All other values
		// in the bundle can only decrease.
		size += bundle7_cbor_uint_sizeof(bundle->payload_block->length);
	} else {
		// Conservative estimation rgd. fragment offset
		size += (
			bundle7_cbor_uint_sizeof(bundle->total_adu_length) -
			bundle7_cbor_uint_sizeof(bundle->fragment_offset)
		);
	}

	return bundle->primary_block_length + size;
}


size_t bundle7_get_last_fragment_min_size(struct bundle *bundle)
{
	size_t size = 0;
	struct bundle_block_list *entry = bundle->blocks;

	while (entry != NULL) {
		if (entry->data->type == BUNDLE_BLOCK_TYPE_PAYLOAD) {
			size += 1 // CBOR array header
			+ bundle7_cbor_uint_sizeof(entry->data->type)
			+ bundle7_cbor_uint_sizeof(entry->data->number)
			+ bundle7_cbor_uint_sizeof(entry->data->flags)
			+ bundle7_cbor_uint_sizeof(entry->data->crc_type)
			+ bundle7_cbor_uint_sizeof(entry->data->length);

			// CRC field
			if (entry->data->crc_type == BUNDLE_CRC_TYPE_32)
				size += 5;
			else if (entry->data->crc_type == BUNDLE_CRC_TYPE_16)
				size += 3;

			// Data length zero + empty byte string
			size += 2;
		}
		// Sum up all blocks that must be replicated in each fragment
		else if (HAS_FLAG(entry->data->flags,
			BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED))
			size += bundle7_block_get_serialized_size(entry->data);

		entry = entry->next;
	}

	// If bundle was not fragmented before, add the "Fragment offset" and
	// "Total Application Data Unit Length" field sizes
	if (!bundle_is_fragmented(bundle)) {
		size += bundle7_cbor_uint_sizeof(bundle->payload_block->length);
		// Conservative estimation for maximum "minimum bundle size",
		// i.e., header length: the fragment offset could have a maximum
		// size corresponding to the fragment offset. All other values
		// in the bundle can only decrease.
		size += bundle7_cbor_uint_sizeof(bundle->payload_block->length);
	} else {
		// Conservative estimation rgd. fragment offset
		size += (
			bundle7_cbor_uint_sizeof(bundle->total_adu_length) -
			bundle7_cbor_uint_sizeof(bundle->fragment_offset)
		);
	}

	return bundle->primary_block_length + size;
}
