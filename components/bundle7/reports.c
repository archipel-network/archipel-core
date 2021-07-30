#include "bundle7/bundle7.h"
#include "bundle7/reports.h"
#include "bundle7/eid.h"
#include "bundle7/create.h"

#include "platform/hal_time.h"

#include "ud3tn/common.h"

#include "cbor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


// --------------------------------
// Administrative Record Generators
// --------------------------------


/**
 * Calculates the length of the last 2 or 4 fields of the Administrative
 * Records "Custody Signals" and "Bundle Status Report".
 *
 * @return number of maximal required bytes for the last fields
 */
static inline size_t record_get_last_fields_size(const struct bundle *bundle)
{
	// Source EID
	size_t payload_size = bundle7_eid_get_max_serialized_size(
		bundle->source);

	// Creation Timestamp
	payload_size += 1 // array header
		+ bundle7_cbor_uint_sizeof(bundle->creation_timestamp_ms)
		+ bundle7_cbor_uint_sizeof(bundle->sequence_number);

	// Fragment information
	if (bundle_is_fragmented(bundle)) {
		payload_size
			+= bundle7_cbor_uint_sizeof(bundle->fragment_offset)
			+ bundle7_cbor_uint_sizeof(
				bundle->payload_block->length);
	}

	return payload_size;
}


/**
 * Serializes the last 2 or 4 fields of the Administrative Records
 * "Custody Signals" and "Bundle Status Report":
 *
 *  - Source EID
 *  - Creation Timestamp
 *
 * If the bundle was fragmented:
 *
 *  - Fragment Offset
 *  - Fragment Length
 *
 * @return Totally written bytes - meaning the previously written bytes
 *         + bytes for the last fields. If an error occures 0 will be returned.
 */
static CborError serialize_last_fields(const struct bundle *bundle,
	CborEncoder *encoder)
{
	CborEncoder timestamp;
	CborError err;

	// Source EID
	err = bundle7_eid_serialize_cbor(bundle->source, encoder);
	if (err)
		return err;

	// Creation Timestamp
	cbor_encoder_create_array(encoder, &timestamp, 2);
	cbor_encode_uint(&timestamp, bundle->creation_timestamp_ms);
	cbor_encode_uint(&timestamp, bundle->sequence_number);
	cbor_encoder_close_container(encoder, &timestamp);

	// Fragment offset and payload length
	if (bundle_is_fragmented(bundle)) {
		cbor_encode_uint(encoder, bundle->fragment_offset);
		cbor_encode_uint(encoder, bundle->payload_block->length);
	}

	return CborNoError;
}


/**
 * Serializes the 5 bundle status information fields in the Bundle Status
 * Report.
 *
 * @return Totally written bytes - meaning the previously written bytes
 *         + bytes for the last fields
 */
static void serialize_status_info(
	const struct bundle *bundle,
	const struct bundle_status_report *report,
	CborEncoder *encoder,
	enum bundle_status_report_status_flags flag)
{
	uint64_t time;
	CborEncoder recursed;

	// Set assertion "true"
	if (HAS_FLAG(report->status, flag)) {
		// Times are reported
		if (HAS_FLAG(bundle->proc_flags,
			BUNDLE_FLAG_REPORT_STATUS_TIME)) {

			cbor_encoder_create_array(encoder, &recursed, 2);
			cbor_encode_boolean(&recursed, true);

			// Find corresponding time
			switch (flag) {
			case BUNDLE_SR_FLAG_BUNDLE_RECEIVED:
				time = report->bundle_received_time;
				break;
			case BUNDLE_SR_FLAG_BUNDLE_FORWARDED:
				time = report->bundle_forwarded_time;
				break;
			case BUNDLE_SR_FLAG_BUNDLE_DELIVERED:
				time = report->bundle_delivered_time;
				break;
			case BUNDLE_SR_FLAG_BUNDLE_DELETED:
				time = report->bundle_deleted_time;
				break;
			default:
				time = 0;
				break;
			}
			cbor_encode_uint(&recursed, time);
		}
		// Times are not reported
		else {
			cbor_encoder_create_array(encoder, &recursed, 1);
			cbor_encode_boolean(&recursed, true);
		}
	}
	// Set assertion "false"
	else {
		cbor_encoder_create_array(encoder, &recursed, 1);
		cbor_encode_boolean(&recursed, false);
	}
	cbor_encoder_close_container(encoder, &recursed);

}


struct bundle *bundle7_generate_status_report(
	const struct bundle * const bundle,
	const struct bundle_status_report *prototype,
	const char *source)
{
	uint8_t *payload;
	CborEncoder container, record, report, status_info;
	CborError err;

	// 5 bytes for:
	//
	// - administrative record array header
	// - administrative record type number
	// - bundle status report array header
	// - status info array header
	// - reason code
	//
	// Status info = 44 bytes
	//
	// - 4x 1 byte array header
	// - 4x 1 byte bool value
	// - 4x 9 bytes uint64_t times
	//
	// + size of last fields
	//
	const size_t payload_size = 49 + record_get_last_fields_size(bundle);

	payload = malloc(payload_size);
	if (payload == NULL)
		return NULL;

	cbor_encoder_init(&container, payload, payload_size, 0);

	// Administrative Record
	cbor_encoder_create_array(&container, &record, 2);
	cbor_encode_uint(&record, BUNDLE_AR_STATUS_REPORT);

	// Bundle Status Report
	cbor_encoder_create_array(&record, &report,
		(bundle_is_fragmented(bundle)) ? 6 : 4);

	// Bundle Status Information
	cbor_encoder_create_array(&report, &status_info, 4);

	serialize_status_info(bundle, prototype, &status_info,
		BUNDLE_SR_FLAG_BUNDLE_RECEIVED);

	serialize_status_info(bundle, prototype, &status_info,
		BUNDLE_SR_FLAG_BUNDLE_FORWARDED);

	serialize_status_info(bundle, prototype, &status_info,
		BUNDLE_SR_FLAG_BUNDLE_DELIVERED);

	serialize_status_info(bundle, prototype, &status_info,
		BUNDLE_SR_FLAG_BUNDLE_DELETED);

	cbor_encoder_close_container(&report, &status_info);

	// Reason Code
	cbor_encode_uint(&report, prototype->reason);

	// Last fields of the report
	err = serialize_last_fields(bundle, &report);
	if (err) {
		free(payload);
		return NULL;
	}

	// Close report and record containers
	cbor_encoder_close_container(&record, &report);
	cbor_encoder_close_container(&container, &record);

	// Compress payload memory
	size_t written = cbor_encoder_get_buffer_size(&container, payload);
	uint8_t *compress = realloc(payload, written);

	if (compress == NULL) {
		free(payload);
		return NULL;
	}

	// Lifetime
	const int64_t lifetime_seconds = (
		(int64_t)bundle_get_expiration_time_s(bundle) -
		(int64_t)hal_time_get_timestamp_s()
	);

	if (lifetime_seconds < 0) {
		free(compress);
		return NULL;
	}

	return bundle7_create_local(
		compress, written, source, bundle->report_to,
		hal_time_get_timestamp_s(), 1,
		lifetime_seconds, BUNDLE_FLAG_ADMINISTRATIVE_RECORD);
}


struct bundle_list *bundle7_generate_custody_signal(
	const struct bundle * const bundle,
	const struct bundle_custody_signal *signal,
	const char *source)
{
	(void)bundle;
	(void)signal;
	(void)source;

	return NULL;
}


// ----------------------------
// Administrative Record Parser
// ----------------------------

typedef enum parser_error {
	ERROR_NONE,
	ERROR_MEMORY,
	ERROR_UNEXPECTED,
	ERROR_CBOR,
	ERROR_UNKNOWN_RECORD_TYPE,
} error_t;


struct record_parser {
	struct bundle_administrative_record *record;
};


// Administrative Record + Generic
static error_t administrative_record(struct record_parser *, CborValue *);
static error_t parse_last_fields(struct record_parser *, CborValue *);

// Bundle Status Report
static error_t status_report(struct record_parser *, CborValue *);
static error_t status_info(struct record_parser *, CborValue *);


error_t administrative_record(struct record_parser *state, CborValue *it)
{
	CborValue nested;
	error_t err;
	uint64_t type;

	// Enter administrative record container
	if (!cbor_value_is_array(it))
		return ERROR_UNEXPECTED;
	if (cbor_value_enter_container(it, &nested))
		return ERROR_CBOR;

	// Record type
	// -----------
	//
	if (!cbor_value_is_unsigned_integer(&nested))
		return ERROR_UNEXPECTED;
	cbor_value_get_uint64(&nested, &type);
	state->record->type = type;

	if (cbor_value_advance_fixed(&nested))
		return ERROR_CBOR;

	switch (state->record->type) {
	case BUNDLE_AR_STATUS_REPORT:
		err = status_report(state, &nested);
		break;
	default:
		return ERROR_UNKNOWN_RECORD_TYPE;
	}

	// An error occured, abort
	if (err)
		return err;

	// Leave administrative record container
	if (!cbor_value_at_end(&nested))
		return ERROR_UNEXPECTED;
	cbor_value_leave_container(it, &nested);

	return ERROR_NONE;
}


error_t parse_last_fields(struct record_parser *state, CborValue *it)
{
	CborValue timestamp;
	char *eid;
	uint64_t number;

	// Bundle Source EID
	// -----------------
	//
	if (bundle7_eid_parse_cbor(it, &eid))
		return ERROR_CBOR;

	state->record->bundle_source_eid_length = strlen(eid);
	state->record->bundle_source_eid = strdup(eid);
	free(eid);

	// Creation Timestamp
	// ------------------
	//
	// Enter timestamp container
	if (!cbor_value_is_array(it))
		return ERROR_UNEXPECTED;
	if (cbor_value_enter_container(it, &timestamp))
		return ERROR_CBOR;

	// Creation time
	if (!cbor_value_is_unsigned_integer(&timestamp))
		return ERROR_UNEXPECTED;

	cbor_value_get_uint64(&timestamp, &number);
	state->record->bundle_creation_timestamp_ms = number;

	if (cbor_value_advance_fixed(&timestamp))
		return ERROR_CBOR;

	// Sequence Number
	if (!cbor_value_is_unsigned_integer(&timestamp))
		return ERROR_UNEXPECTED;

	cbor_value_get_uint64(&timestamp, &number);
	state->record->bundle_sequence_number = number;

	// Leave timestamp container
	if (cbor_value_advance_fixed(&timestamp))
		return ERROR_CBOR;
	if (!cbor_value_at_end(&timestamp))
		return ERROR_UNEXPECTED;
	if (cbor_value_leave_container(it, &timestamp))
		return ERROR_CBOR;

	// No fragmentation
	if ((state->record->flags & BUNDLE_AR_FLAG_FRAGMENT) == 0)
		return ERROR_NONE;

	// Fragment offset
	// ---------------
	//
	if (!cbor_value_is_unsigned_integer(it))
		return ERROR_UNEXPECTED;

	cbor_value_get_uint64(it, &number);
	state->record->fragment_offset = number;

	if (cbor_value_advance_fixed(it))
		return ERROR_CBOR;

	// Fragment Length
	// ---------------
	//
	if (!cbor_value_is_unsigned_integer(it))
		return ERROR_UNEXPECTED;

	cbor_value_get_uint64(it, &number);
	state->record->fragment_length = number;

	if (cbor_value_advance_fixed(it))
		return ERROR_CBOR;

	return ERROR_NONE;
}


// Bundle Status Report
// --------------------

error_t status_report(struct record_parser *state, CborValue *it)
{
	CborValue report;
	error_t err;
	size_t length;
	uint64_t reason;

	if (!cbor_value_is_array(it) || !cbor_value_is_length_known(it))
		return ERROR_UNEXPECTED;

	if (cbor_value_get_array_length(it, &length) != CborNoError)
		return ERROR_UNEXPECTED;

	// 4 items if reported bundle was not fragmented
	// 6 items if reported bundle was fragmented
	if (length == 6)
		state->record->flags |= BUNDLE_AR_FLAG_FRAGMENT;
	else if (length != 4)
		return ERROR_UNEXPECTED;

	if (cbor_value_enter_container(it, &report))
		return ERROR_CBOR;

	// Allocate the bundle status report
	state->record->status_report = malloc(
		sizeof(struct bundle_status_report));

	if (state->record->status_report == NULL)
		return ERROR_MEMORY;

	state->record->status_report->status = 0;

	// Status Information
	// ------------------
	//
	err = status_info(state, &report);
	if (err)
		return err;

	// Reason Code
	// -----------
	//
	if (!cbor_value_is_unsigned_integer(&report))
		return ERROR_UNEXPECTED;

	cbor_value_get_uint64(&report, &reason);
	state->record->status_report->reason = reason;

	if (cbor_value_advance_fixed(&report))
		return ERROR_CBOR;

	err = parse_last_fields(state, &report);
	if (err)
		return err;

	// Leave Report container
	if (!cbor_value_at_end(&report))
		return ERROR_UNEXPECTED;
	if (cbor_value_leave_container(it, &report))
		return ERROR_CBOR;

	return ERROR_NONE;
}


error_t status_info(struct record_parser *state, CborValue *it)
{
	CborValue asserts, assertion;
	size_t length;

	if (!cbor_value_is_array(it) || !cbor_value_is_length_known(it))
		return ERROR_UNEXPECTED;

	// Check length of status info array
	if (cbor_value_get_array_length(it, &length) != CborNoError)
		return ERROR_UNEXPECTED;
	if (length != 4)
		return ERROR_UNEXPECTED;

	// Enter assertion array
	if (cbor_value_enter_container(it, &asserts))
		return ERROR_CBOR;

	for (int i = 0; i < 4; ++i) {
		bool value;
		uint64_t time;

		// Enter assertion
		if (!cbor_value_is_array(&asserts))
			return ERROR_UNEXPECTED;
		if (cbor_value_enter_container(&asserts, &assertion))
			return ERROR_CBOR;

		enum bundle_status_report_status_flags flag;

		switch (i) {
		case 0:
			flag = BUNDLE_SR_FLAG_BUNDLE_RECEIVED;
			break;
		case 1:
			flag = BUNDLE_SR_FLAG_BUNDLE_FORWARDED;
			break;
		case 2:
			flag = BUNDLE_SR_FLAG_BUNDLE_DELIVERED;
			break;
		case 3:
			flag = BUNDLE_SR_FLAG_BUNDLE_DELETED;
			break;
		default:
			return ERROR_UNEXPECTED;
		}

		// Value (true / false)
		if (!cbor_value_is_boolean(&assertion))
			return ERROR_UNEXPECTED;
		cbor_value_get_boolean(&assertion, &value);

		// Set flag if assertion was true
		if (value)
			state->record->status_report->status |= flag;

		// next
		if (cbor_value_advance_fixed(&assertion))
			return ERROR_CBOR;

		// Time
		if (!cbor_value_at_end(&assertion)) {
			if (!cbor_value_is_unsigned_integer(&assertion))
				return ERROR_UNEXPECTED;
			cbor_value_get_uint64(&assertion, &time);

			switch (flag) {
			case BUNDLE_SR_FLAG_BUNDLE_RECEIVED:
				state->record->status_report
					->bundle_received_time = time;
				break;
			case BUNDLE_SR_FLAG_BUNDLE_FORWARDED:
				state->record->status_report
					->bundle_forwarded_time = time;
				break;
			case BUNDLE_SR_FLAG_BUNDLE_DELIVERED:
				state->record->status_report
					->bundle_delivered_time = time;
				break;
			case BUNDLE_SR_FLAG_BUNDLE_DELETED:
				state->record->status_report
					->bundle_deleted_time = time;
				break;
			// Should never happen ...
			default:
				break;
			}

			if (cbor_value_advance_fixed(&assertion))
				return ERROR_CBOR;
		}

		// Leave assertion
		if (!cbor_value_at_end(&assertion))
			return ERROR_UNEXPECTED;
		if (cbor_value_leave_container(&asserts, &assertion))
			return ERROR_CBOR;
	}

	// Leave assertion array
	if (cbor_value_leave_container(it, &asserts))
		return ERROR_CBOR;

	return ERROR_NONE;
}

struct bundle_administrative_record *bundle7_parse_administrative_record(
	const uint8_t *const data, const size_t length)
{
	// Strange case if a bundle was sent with an empty payload
	if (length == 0)
		return NULL;

	// Initialize administrative record
	struct bundle_administrative_record *record = malloc(
		sizeof(struct bundle_administrative_record));

	// Could not allocate enough memory
	if (record == NULL)
		return NULL;

	record->flags = 0;
	record->event_timestamp   = 0;
	record->event_nanoseconds = 0;
	record->custody_signal = NULL;
	record->status_report = NULL;

	struct record_parser state = {
		.record = record,
	};
	CborParser parser;
	CborValue it;
	CborError err;

	err = cbor_parser_init(
		data,
		length,
		0, &parser, &it
	);
	if (err) {
		free(record);
		return NULL;
	}

	if (administrative_record(&state, &it)) {
		free(record);
		return NULL;
	}

	return record;
}
