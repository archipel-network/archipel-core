#include "cla/mtcp_proto.h"

#include "platform/hal_io.h"

#include "ud3tn/common.h"
#include "ud3tn/parser.h"

#include "cbor.h"

#include <stddef.h>
#include <stdint.h>

void mtcp_parser_reset(struct parser *mtcp_parser)
{
	mtcp_parser->status = PARSER_STATUS_GOOD;
	mtcp_parser->next_bytes = 0;
	mtcp_parser->flags = PARSER_FLAG_NONE;
}

size_t mtcp_parser_parse(struct parser *mtcp_parser,
			 const uint8_t *buffer,
			 size_t length)
{
	CborParser parser;
	CborValue it;
	CborError err;
	size_t pl_length = 0;

	err = cbor_parser_init(buffer, length, 0, &parser, &it);
	if (err == CborNoError && !cbor_value_is_byte_string(&it))
		err = CborErrorIllegalType;
	if (err == CborNoError)
		err = cbor_value_get_string_length(&it, &pl_length);
	if (err == CborErrorUnexpectedEOF) {
		// We need more data!
		return 0;
	} else if (err != CborNoError) {
		LOG("mtcp: Invalid CBOR byte string header provided.");
		// Skip 1 byte
		return 1;
	}

	mtcp_parser->flags = PARSER_FLAG_DATA_SUBPARSER;
	mtcp_parser->next_bytes = pl_length;

	// See block_data(...) in the bundle7 parser for a detailed
	// explanation of what happens here.
	it.type = CborIntegerType;
	// NOTE: Intentionally no error handling, see bundle7 parser!
	cbor_value_advance_fixed(&it);

	return cbor_value_get_next_byte(&it) - buffer;
}

size_t mtcp_encode_header(uint8_t *const buffer, const size_t buffer_size,
			  const size_t data_length)
{
	CborEncoder encoder;

	ASSERT(buffer_size >= 9);
	cbor_encoder_init(&encoder, buffer, buffer_size, 0);
	ASSERT(cbor_encode_uint(&encoder, data_length) == CborNoError);

	const size_t hdr_len = cbor_encoder_get_buffer_size(&encoder, buffer);

	ASSERT(hdr_len != 0);
	buffer[0] |= 0x40; // CBOR uint -> byte string

	return hdr_len;
}
