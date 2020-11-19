#include "bundle7/bundle7.h"
#include "bundle7/eid.h"

#include "ud3tn/bundle.h"

#include "cbor.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// --------------------------------
// Endpoint Identifier (EID) Parser
// --------------------------------

static inline size_t decimal_digits(uint32_t number)
{
	size_t digits = 0;

	while (number != 0) {
		digits++;
		number /= 10;
	}
	return digits;
}

static CborError eid_parse_dtn(CborValue *it, char **eid);
static CborError eid_parse_ipn(CborValue *it, char **eid);


CborError bundle7_eid_parse_cbor(CborValue *it, char **eid)
{
	CborValue recursed;
	CborError err;
	size_t length;
	uint64_t schema;

	if (!cbor_value_is_array(it) || !cbor_value_is_length_known(it))
		return CborErrorIllegalType;

	// Array must contain 2 items
	if (cbor_value_get_array_length(it, &length) != CborNoError)
		return CborErrorIllegalType;
	if (length != 2)
		return CborErrorIllegalType;

	// Enter EID array
	err = cbor_value_enter_container(it, &recursed);
	if (err)
		return err;

	// EID schema (first element)
	if (!cbor_value_is_unsigned_integer(&recursed))
		return CborErrorIllegalType;

	cbor_value_get_uint64(&recursed, &schema);

	// Second item (schema specific)
	err = cbor_value_advance_fixed(&recursed);
	if (err)
		return err;

	// Call schema specific parsing functions
	switch (schema) {
	case BUNDLE_V7_EID_SCHEMA_DTN:
		err = eid_parse_dtn(&recursed, eid);
		break;
	case BUNDLE_V7_EID_SCHEMA_IPN:
		err = eid_parse_ipn(&recursed, eid);
		break;
	// unknown schema
	default:
		return CborErrorIllegalType;
	}

	// SSP parsing returned error
	if (err)
		return err;

	assert(cbor_value_at_end(&recursed));

	// Leave EID array
	err = cbor_value_leave_container(it, &recursed);
	if (err) {
		free(*eid);
		return err;
	}

	return CborNoError;
}


CborError eid_parse_dtn(CborValue *it, char **eid)
{
	CborError err;
	size_t length;
	uint64_t num;

	// Special case:
	//
	//     null-EID ("dtn:none")
	//
	if (cbor_value_is_unsigned_integer(it)) {
		cbor_value_get_uint64(it, &num);

		// The only allowd value here is zero
		if (num != 0)
			return CborErrorIllegalType;

		// Go to end of array
		cbor_value_advance_fixed(it);

		// Allocate output buffer
		*eid = malloc(9);
		if (*eid == NULL)
			return CborErrorOutOfMemory;

		memcpy(*eid, "dtn:none", 9);
		return CborNoError;
	}

	// The SSP of the "dtn" schema is a CBOR text string
	if (!cbor_value_is_text_string(it))
		return CborErrorIllegalType;

	// Determine SSP length. We do not check if the length is known because
	// this is already done in cbor_value_get_string_length(). If the
	// length is unknown a CborErrorUnknownLength will be returned
	err = cbor_value_get_string_length(it, &length);
	if (err)
		return err;

	// Remaining bytes are not sufficient
	if ((size_t) (it->parser->end - it->ptr) < length)
		return CborErrorUnexpectedEOF;

	// The number of bytes written to the buffer will be written to the
	// length variable
	length += 1; // '\0' termination

	// Allocate output buffer for "dtn:" prefix + SSP
	*eid = malloc(4 + length);
	if (*eid == NULL)
		return CborErrorOutOfMemory;

	// Copy prefix
	memcpy(*eid, "dtn:", 4);

	// Copy SSP + '\0' and advance iterator to next element after SSP.
	err = cbor_value_copy_text_string(it, *eid + 4, &length, it);
	if (err) {
		free(*eid);
		return err;
	}

	return CborNoError;
}


CborError eid_parse_ipn(CborValue *it, char **eid)
{
	CborValue recursed;
	CborError err;
	uint64_t nodenum, servicenum;

	// Enter array
	if (!cbor_value_is_array(it) || !cbor_value_is_length_known(it))
		return CborErrorIllegalType;

	err = cbor_value_enter_container(it, &recursed);
	if (err)
		return err;

	// Node number
	// -----------
	if (!cbor_value_is_unsigned_integer(&recursed))
		return CborErrorIllegalType;

	cbor_value_get_uint64(&recursed, &nodenum);

	// Advance to service number
	err = cbor_value_advance_fixed(&recursed);
	if (err)
		return err;

	// Service number
	// --------------
	if (!cbor_value_is_unsigned_integer(&recursed))
		return CborErrorIllegalType;

	cbor_value_get_uint64(&recursed, &servicenum);

	err = cbor_value_advance_fixed(&recursed);
	if (err)
		return err;

	// Expect end of array
	if (!cbor_value_at_end(&recursed))
		return CborErrorIllegalType;


	// Leave array
	err = cbor_value_leave_container(it, &recursed);
	if (err)
		return err;

	// "ipn:" + nodenum + "." + servicenum + "\0"
	size_t length = 4
			+ decimal_digits(nodenum)
			+ 1
			+ decimal_digits(servicenum)
			+ 1;

	// Allocate string memory
	*eid = malloc(length);
	if (*eid == NULL)
		return CborErrorOutOfMemory;

	// We use snprintf() because sprintf() would use a different
	// malloc() memory allocator then the bundle7 library.
	snprintf(*eid, length, "ipn:%"PRIu64".%"PRIu64, nodenum, servicenum);

	return CborNoError;
}



char *bundle7_eid_parse(const uint8_t *buffer, size_t length)
{
	char *eid = NULL;
	CborValue it;
	CborParser parser;
	CborError err;

	err = cbor_parser_init(buffer, length, 0, &parser, &it);
	if (err != CborNoError)
		return NULL;

	err = bundle7_eid_parse_cbor(&it, &eid);
	if (err != CborNoError)
		return NULL;

	return eid;
}


// ------------------------------------
// Endpoint Identifier (EID) Serializer
// ------------------------------------

//static const uint8_t _dtn_none[3] = {
//	0x82, 0x01, 0x00
//};

CborError serialize_dtn_none(CborEncoder *encoder)
{
	CborEncoder recursed;

	cbor_encoder_create_array(encoder, &recursed, 2);
	cbor_encode_uint(&recursed, BUNDLE_V7_EID_SCHEMA_DTN);
	cbor_encode_uint(&recursed, 0);
	cbor_encoder_close_container(encoder, &recursed);

	return CborNoError;
}


CborError serialize_dtn(const char *eid, CborEncoder *encoder)
{
	CborEncoder recursed;
	size_t ssp_length = strlen(eid) - 4;

	// EID container
	cbor_encoder_create_array(encoder, &recursed, 2);

	// Schema
	cbor_encode_uint(&recursed, BUNDLE_V7_EID_SCHEMA_DTN);

	// SSP
	cbor_encode_text_string(&recursed, eid + 4, ssp_length);

	// EID container
	cbor_encoder_close_container(encoder, &recursed);

	return CborNoError;
}


CborError serialize_ipn(const char *eid, CborEncoder *encoder)
{
	CborEncoder inner, ssp;
	uint32_t nodenum, servicenum;

	// Parse node and service numbers
	if (sscanf(eid,	"ipn:%"PRIu32".%"PRIu32, &nodenum, &servicenum) != 2)
		return CborErrorIllegalType;

	// EID container
	cbor_encoder_create_array(encoder, &inner, 2);

	// Schema
	cbor_encode_uint(&inner, BUNDLE_V7_EID_SCHEMA_IPN);

	// SSP
	cbor_encoder_create_array(&inner, &ssp, 2);
	cbor_encode_uint(&ssp, nodenum);
	cbor_encode_uint(&ssp, servicenum);
	cbor_encoder_close_container(&inner, &ssp);

	// EID container
	cbor_encoder_close_container(encoder, &inner);

	return CborNoError;
}


size_t bundle7_eid_get_max_serialized_size(const char *eid)
{
	// We use "string length + cbor_encode_uint(string length - 4)" as an
	// upper bound of the required buffer size.
	//
	// schema:
	//     "ipn:" / "dtn:" > 1byte CBOR array header + 1byte schema number
	//
	//     This means that there are 2 byte remaining unused.
	//
	// ipn:
	//     SSP string representation is always longer than numerical
	//     representation. The dot "." string literal reservers the space
	//     for the CBOR array header
	//
	// dtn:
	//     The CBOR encoded SSP requires an header encoding the string
	//     length. Therefore we add the size of this header to the bound.
	//     The SSP is "string length - 4" bytes long.
	//
	const size_t length = strlen(eid);

	return length + bundle7_cbor_uint_sizeof(length);
}


uint8_t *bundle7_eid_serialize_alloc(const char *eid, size_t *length)
{
	size_t buffer_size = bundle7_eid_get_max_serialized_size(eid);
	uint8_t *buffer = malloc(buffer_size);

	// We could not allocate enough memory
	if (buffer == NULL)
		return NULL;

	int written = bundle7_eid_serialize(eid, buffer, buffer_size);

	if (written <= 0) {
		free(buffer);
		return NULL;
	}

	// Shorten buffer to correct length
	uint8_t *compress = realloc(buffer, written);

	// Something went south during realloc, clear buffer and abort
	if (compress == NULL) {
		free(buffer);
		return NULL;
	}

	*length = (size_t) written;
	return compress;
}

CborError bundle7_eid_serialize_cbor(const char *eid, CborEncoder *encoder)
{
	size_t str_length = strlen(eid);

	// Special case: null-EID
	if (str_length == 8 && strncmp(eid, "dtn:none", 8) == 0)
		return serialize_dtn_none(encoder);
	// DTN (allows empty SSPs)
	if (str_length >= 4 && strncmp(eid, "dtn:", 4) == 0)
		return serialize_dtn(eid, encoder);
	// IPN
	else if (str_length > 4 && strncmp(eid, "ipn:", 4) == 0)
		return serialize_ipn(eid, encoder);
	// Unknwon schema
	else
		return CborErrorIllegalType;
}

int bundle7_eid_serialize(const char *eid, uint8_t *buffer, size_t buffer_size)
{
	CborEncoder encoder;
	CborError err;

	cbor_encoder_init(&encoder, buffer, buffer_size, 0);
	err = bundle7_eid_serialize_cbor(eid, &encoder);

	// A non-recoverable error occured
	if (err != CborNoError && err != CborErrorOutOfMemory)
		return -1;

	// Out of memory (buffer too small)
	if (cbor_encoder_get_extra_bytes_needed(&encoder) > 0)
		return 0;

	return cbor_encoder_get_buffer_size(&encoder, buffer);
}
