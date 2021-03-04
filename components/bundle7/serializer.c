#include "bundle7/eid.h"
#include "bundle7/serializer.h"

#include "cbor.h"
#include "compilersupport_p.h" // Private TinyCBOR header, used for endianess

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/hal_io.h"

/* CBOR output buffer size. This buffer will be allocated during the
 * serialization process and released afterwards
 */
#define BUFFER_SIZE 128


static inline size_t primary_block_get_item_count(struct bundle *bundle)
{
	size_t length = 8;

	if (bundle->crc_type != BUNDLE_CRC_TYPE_NONE)
		length++;

	if (bundle_is_fragmented(bundle))
		length += 2;

	return length;
}


static inline size_t block_get_item_count(const struct bundle_block *block)
{
	size_t length = 5;

	if (block->crc_type != BUNDLE_CRC_TYPE_NONE)
		length++;

	return length;
}


static CborError write_crc(
	struct CborEncoder *encoder,
	enum bundle_crc_type crc_type, struct crc_stream *crc)
{
	// CRC-32
	if (crc_type == BUNDLE_CRC_TYPE_32) {
		// Feed the "zero" CRC checksum
		crc->feed(crc, 0x44);
		crc->feed(crc, 0x00);
		crc->feed(crc, 0x00);
		crc->feed(crc, 0x00);
		crc->feed(crc, 0x00);
		crc->feed_eof(crc);

		// Swap to network byte order
		crc->checksum = cbor_htonl(crc->checksum);

		return cbor_encode_byte_string(encoder, crc->bytes, 4);
	}
	// CRC-16
	else {
		// Feed the "zero" CRC checksum
		crc->feed(crc, 0x42);
		crc->feed(crc, 0x00);
		crc->feed(crc, 0x00);
		crc->feed_eof(crc);

		// Swap to network byte order
		crc->checksum = cbor_htons(crc->checksum);

		// We skip the first two (empty) bytes of the checksum here
		return cbor_encode_byte_string(encoder, crc->bytes, 2);
	}
}


/*
 * Number of bytes required for CBOR-encoded max. value:
 *
 * Primary Block
 * -------------
 *
 *
 * Extension Block (without payload):
 * ----------------------------------
 *
 * Array header:                   1 Byte
 * Block type code (255):          2 Byte
 * Block number (255):             2 Byte
 * Block processing control flags: 1 Byte
 * CRC (32 bit):                   5 Byte
 * Block data length:              5 Byte
 */


static inline void init_crc(struct crc_stream *crc,
	enum bundle_crc_type type)
{
	if (type == BUNDLE_CRC_TYPE_32)
		crc_init(crc, CRC32);
	else if (type == BUNDLE_CRC_TYPE_16)
		crc_init(crc, CRC16_X25);
}


static inline void feed_crc(struct crc_stream *crc,
	enum bundle_crc_type type, uint8_t *buffer, size_t length)
{
	if (type != BUNDLE_CRC_TYPE_NONE)
		crc_feed_bytes(crc, buffer, length);
}

static uint32_t bundle7_filter_protocol_proc_flags(const struct bundle *bundle)
{
	uint32_t flags = bundle->proc_flags & BP_V7_FLAGS;
	return flags;
}

enum ud3tn_result bundle7_serialize(
	struct bundle *bundle,
	void (*write)(void *cla_obj, const void *, const size_t),
	void *cla_obj)
{
	// Assert that the bundle has correct version
	if (bundle->protocol_version != 7)
		return UD3TN_FAIL;

	uint8_t *buffer;
	CborEncoder encoder;
	struct crc_stream crc;
	int written;

	buffer = malloc(BUFFER_SIZE);
	if (buffer == NULL)
		return UD3TN_FAIL;

	// Bundle start (CBOR indefinite array)
	buffer[0] = 0x9f;

	// -------------
	// Primary Block
	// -------------

	buffer[1] = 0x80 + primary_block_get_item_count(bundle);

	init_crc(&crc, bundle->crc_type);

	cbor_encoder_init(&encoder, buffer + 2, BUFFER_SIZE - 2, 0);
	cbor_encode_uint(&encoder, bundle->protocol_version);
	cbor_encode_uint(&encoder,
		bundle7_filter_protocol_proc_flags(bundle));
	cbor_encode_uint(&encoder, bundle->crc_type);

	write(cla_obj, buffer, cbor_encoder_get_buffer_size(&encoder, buffer));
	feed_crc(&crc, bundle->crc_type, buffer + 1,
		cbor_encoder_get_buffer_size(&encoder, buffer + 1));

	// Destination EID
	written = bundle7_eid_serialize(bundle->destination,
		buffer, BUFFER_SIZE);
	if (written <= 0)
		return UD3TN_FAIL;
	write(cla_obj, buffer, written);
	feed_crc(&crc, bundle->crc_type, buffer, written);

	// Source EID
	written = bundle7_eid_serialize(bundle->source,
		buffer, BUFFER_SIZE);
	if (written <= 0)
		return UD3TN_FAIL;
	write(cla_obj, buffer, written);
	feed_crc(&crc, bundle->crc_type, buffer, written);

	// Report-To EID
	written = bundle7_eid_serialize(bundle->report_to,
		buffer, BUFFER_SIZE);
	if (written <= 0)
		return UD3TN_FAIL;
	write(cla_obj, buffer, written);
	feed_crc(&crc, bundle->crc_type, buffer, written);

	// Creation Timestamp
	buffer[0] = 0x82;
	cbor_encoder_init(&encoder, buffer + 1, BUFFER_SIZE - 1, 0);
	cbor_encode_uint(&encoder, bundle->creation_timestamp_ms);
	cbor_encode_uint(&encoder, bundle->sequence_number);
	cbor_encode_uint(&encoder, bundle->lifetime_ms);

	if (bundle_is_fragmented(bundle)) {
		cbor_encode_uint(&encoder, bundle->fragment_offset);
		cbor_encode_uint(&encoder, bundle->total_adu_length);
	}

	// CRC checksum for primary block
	if (bundle->crc_type != BUNDLE_CRC_TYPE_NONE) {
		feed_crc(&crc, bundle->crc_type, buffer,
			cbor_encoder_get_buffer_size(&encoder, buffer));

		// Calculate and encode CRC checksum for primary block.
		write_crc(&encoder, bundle->crc_type, &crc);
	}

	write(cla_obj, buffer, cbor_encoder_get_buffer_size(&encoder, buffer));

	// ----------------
	// Extension Blocks
	// ----------------

	struct bundle_block_list *cur_block = bundle->blocks;

	while (cur_block != NULL) {
		const struct bundle_block *block = cur_block->data;

		init_crc(&crc, block->crc_type);

		// CBOR array header with embedded number of items
		buffer[0] = 0x80 + block_get_item_count(block);

		cbor_encoder_init(&encoder, buffer + 1, BUFFER_SIZE - 1, 0);
		cbor_encode_uint(&encoder, block->type);
		cbor_encode_uint(&encoder, block->number);
		cbor_encode_uint(&encoder,
			bundle7_convert_to_protocol_block_flags(
				block));
		cbor_encode_uint(&encoder, block->crc_type);

		const size_t bytes_before_length = cbor_encoder_get_buffer_size(
			&encoder, buffer
		);

		// As the byte string length is represented in the same manner
		// as a uint in CBOR, we can write it like that and afterwards
		// change the type code to byte string.
		cbor_encode_uint(&encoder, block->length);
		buffer[bytes_before_length] |= 0x40; // uint -> bytestring

		write(cla_obj, buffer, cbor_encoder_get_buffer_size(&encoder,
								    buffer));
		feed_crc(&crc, block->crc_type, buffer,
			 cbor_encoder_get_buffer_size(&encoder, buffer));

		write(cla_obj, block->data, block->length);
		feed_crc(&crc, block->crc_type,
			 block->data, block->length);

		if (block->crc_type != BUNDLE_CRC_TYPE_NONE) {
			// Reset CBOR encoder
			cbor_encoder_init(&encoder, buffer, BUFFER_SIZE, 0);

			// Calculate and CRC checksum for extension block
			write_crc(&encoder, block->crc_type, &crc);

			write(cla_obj, buffer,
			      cbor_encoder_get_buffer_size(&encoder, buffer));
		}

		cur_block = cur_block->next;
	}

	// CBOR "break"
	buffer[0] = 0xff;
	write(cla_obj, buffer, 1);

	free(buffer);

	return UD3TN_OK;
}
