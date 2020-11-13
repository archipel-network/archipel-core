#include "bundle7/hopcount.h"

#include "cbor.h"

bool bundle7_hop_count_parse(struct bundle_hop_count *hop_count,
	const uint8_t *buffer, size_t length)
{
	CborParser parser;
	CborValue it;
	CborError err;
	uint64_t number;

	err = cbor_parser_init(buffer, length, 0, &parser, &it);
	if (err || !cbor_value_is_array(&it))
		return false;

	cbor_value_enter_container(&it, &it);

	//
	// Hop Limit
	//
	if (!cbor_value_is_unsigned_integer(&it))
		return false;

	cbor_value_get_uint64(&it, &number);
	hop_count->limit = (uint16_t) number;

	// next item
	err = cbor_value_advance_fixed(&it);
	if (err)
		return false;

	//
	// Hop Count
	//
	if (!cbor_value_is_unsigned_integer(&it))
		return false;

	cbor_value_get_uint64(&it, &number);
	hop_count->count = (uint16_t) number;

	return true;
}


size_t bundle7_hop_count_serialize(const struct bundle_hop_count *hop_count,
	uint8_t *buffer, size_t length)
{
	CborEncoder encoder, recursed;

	if (length < BUNDLE7_HOP_COUNT_MAX_ENCODED_SIZE)
		return 0;

	cbor_encoder_init(&encoder, buffer, length, 0);

	cbor_encoder_create_array(&encoder, &recursed, 2);
	cbor_encode_uint(&recursed, hop_count->limit);
	cbor_encode_uint(&recursed, hop_count->count);
	cbor_encoder_close_container(&encoder, &recursed);

	return cbor_encoder_get_buffer_size(&encoder, buffer);
}
