#include "bundle7/bundle_age.h"

#include "cbor.h"

bool bundle_age_parse(uint64_t *bundle_age, const uint8_t *buffer,
	size_t length)
{
	CborParser parser;
	CborValue it;

	if (cbor_parser_init(buffer, length, 0, &parser, &it) ||
	    !cbor_value_is_unsigned_integer(&it))
		return false;

	cbor_value_get_uint64(&it, bundle_age);

	return true;
}


size_t bundle_age_serialize(const uint64_t bundle_age, uint8_t *const buffer,
	const size_t length)
{
	CborEncoder encoder;

	if (length < BUNDLE_AGE_MAX_ENCODED_SIZE)
		return 0;

	cbor_encoder_init(&encoder, buffer, length, 0);
	cbor_encode_uint(&encoder, bundle_age);

	return cbor_encoder_get_buffer_size(&encoder, buffer);
}
