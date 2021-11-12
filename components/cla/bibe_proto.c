#include "cla/bibe_proto.h"

#include "platform/hal_io.h"

#include "bundle7/reports.h"

#include "ud3tn/common.h"
#include "ud3tn/parser.h"
#include "cbor.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

void bibe_parser_reset(struct parser *bibe_parser)
{
	bibe_parser->status = PARSER_STATUS_GOOD;
	bibe_parser->next_bytes = 0;
	bibe_parser->flags = PARSER_FLAG_NONE;
}

size_t bibe_parser_parse(const uint8_t *buffer,
			 size_t length,
			 struct bibe_protocol_data_unit *bpdu)
{
	CborParser parser;
	CborError err;
	CborValue it;
	CborValue report;
	size_t array_length;
	uint64_t transmission_id;
	uint64_t retransmission_time;
	
	if (bpdu == NULL)
		return CborErrorInternalError;

	err = cbor_parser_init(buffer, length, 0, &parser, &it);
	
	if(err){
		return err;
	}

	if (!cbor_value_is_array(&it) || !cbor_value_is_length_known(&it))
		return CborErrorIllegalType;

	if (cbor_value_get_array_length(&it, &array_length) != CborNoError)
		return CborErrorUnknownLength;

	// 3 items (transmission id, retransmission time, encapsulated bundle)
    if (array_length < 3)
		return CborErrorTooFewItems;
	else if (array_length > 3)
	{
		return CborErrorTooManyItems;
	}

	if (cbor_value_enter_container(&it, &report))
		return CborErrorInternalError;

	// Transmission ID
	// ---------------
	if(!cbor_value_is_unsigned_integer(&report))
		return CborErrorIllegalType;
	cbor_value_get_uint64(&report, &transmission_id);
	bpdu->transmission_id = transmission_id;
	if (cbor_value_advance_fixed(&report))
		return CborErrorUnexpectedEOF;

	// Retransmission time
	// -------------------
	if(!cbor_value_is_unsigned_integer(&report))
		return CborErrorIllegalType;
	cbor_value_get_uint64(&report, &retransmission_time);
	bpdu->retransmission_time = retransmission_time;
	if (cbor_value_advance_fixed(&report))
		return CborErrorUnexpectedEOF;

	// Encapsulated bundle
	// -------------------
	if (!cbor_value_is_byte_string(&report))
		return CborErrorIllegalType;
	
	uint64_t bundle_str_len;
	cbor_value_get_string_length(&report, &bundle_str_len);
	//allocate memory for the encapsulated bundle
	bpdu->encapsulated_bundle = malloc(bundle_str_len);
	bpdu->payload_length = bundle_str_len;
	// From the cbor docs:
	//   "The next pointer, if not null, will be updated to point to the next item after 
	//    this string. If value points to the last item, then next will be invalid."
	// Since we don't have a next element, we need to pass a null pointer to the function here.
	cbor_value_copy_byte_string(
		&report,
		bpdu->encapsulated_bundle,
		&bundle_str_len,
		NULL
		);
	if (cbor_value_advance(&report))
		return CborErrorUnexpectedEOF;

	// Leave BPDU container
	if (!cbor_value_at_end(&report))
		return CborErrorInternalError;
	if (cbor_value_leave_container(&it, &report))
		return CborErrorInternalError;

	return 0;

}