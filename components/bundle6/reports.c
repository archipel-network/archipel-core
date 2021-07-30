#include "bundle6/create.h"
#include "bundle6/reports.h"
#include "bundle6/sdnv.h"

#include "platform/hal_time.h"

#include "ud3tn/common.h"
#include "ud3tn/parser.h"
#include "ud3tn/report_manager.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// -------------------------------
// Administrative Record Generator
// -------------------------------

static struct bundle *encapsulate_record(
	const struct bundle * const bundle,
	const char *source_eid, const char *dest_eid,
	uint8_t *payload, const int payload_len)
{
	// Lifetime
	const int64_t lifetime_seconds = (
		(int64_t)bundle_get_expiration_time_s(bundle) -
		(int64_t)hal_time_get_timestamp_s()
	);

	if (lifetime_seconds < 0) {
		// NOTE: payload is freed by the create function
		return NULL;
	}

	struct bundle *result = bundle6_create_local(
		payload, payload_len,
		source_eid, dest_eid,
		hal_time_get_timestamp_s(), 1,
		lifetime_seconds, BUNDLE_FLAG_ADMINISTRATIVE_RECORD);

	if (result == NULL) {
		// NOTE: payload is freed by the create function
		return NULL;
	}

	result->ret_constraints = BUNDLE_RET_CONSTRAINT_FLAG_OWN;

	return result;
}


#define LENGTH_MAX_SIZE 4
#define DTN_TIME_MAX_SIZE 9
#define SEQ_NUM_MAX_SIZE 4
#define EID_LENGTH_MAX_SIZE 2
#define EID_DEFAULT_LENGTH 40

#define ADMINISTRATIVE_HEADER_SIZE 1
#define ADMINISTRATVE_RECORD_MAX_SIZE \
	(ADMINISTRATIVE_HEADER_SIZE \
	+ 2 + (LENGTH_MAX_SIZE * 2) + (DTN_TIME_MAX_SIZE * 2) \
	+ SEQ_NUM_MAX_SIZE + EID_LENGTH_MAX_SIZE + EID_DEFAULT_LENGTH)


static struct bundle *generate_record(
		const struct bundle * const bundle, const char *source_eid,
		const char *dest_eid, const bool is_custody_signal,
		const uint8_t prefix_1, const uint8_t prefix_2)
{
	uint8_t *buffer = (uint8_t *)malloc(ADMINISTRATVE_RECORD_MAX_SIZE);
	uint8_t *cur = buffer;
	bool fragment;
	uint16_t eid_length, cur_length;
	uint64_t millitime;
	struct bundle *ret;

	if (buffer == NULL)
		return NULL;

	/* Write record type, flags, prefixes (status flags, reason, ...) */
	fragment = bundle_is_fragmented(bundle);
	(*cur++) = (is_custody_signal ? 0x20 : 0x10) | (fragment ? 0x01 : 0x00);
	(*cur++) = prefix_1;
	if (is_custody_signal)
		(*cur++) = prefix_2;
	/* Write fragment info if present */
	if (fragment) {
		cur += sdnv_write_u32(cur, bundle->fragment_offset);
		cur += sdnv_write_u32(cur, bundle->payload_block->length);
	}
	/* Use the current time as generation time, append bundle times */
	millitime = hal_time_get_timestamp_ms();
	/* Add a "DTN time": 1) TS, 2) Nanoseconds since start of cur. second */
	cur += sdnv_write_u64(cur, (millitime / 1000));
	cur += sdnv_write_u32(cur, (millitime % 1000) * 1000); /* "ns" */
	/* Copy bundle data */
	cur += sdnv_write_u64(cur, bundle->creation_timestamp_ms / 1000);
	cur += sdnv_write_u64(cur, bundle->sequence_number);
	/* Bundle source EID (length + data) */
	eid_length = strlen(bundle->source);
	cur += sdnv_write_u32(cur, eid_length);
	cur_length = cur - buffer;
	buffer = realloc(buffer, cur_length + eid_length);
	cur = buffer + cur_length;
	memcpy(cur, bundle->source, eid_length);
	cur += eid_length;
	/* Build the bundle around our generated payload */
	ret = encapsulate_record(
		bundle,
		source_eid,
		dest_eid,
		buffer,
		cur - buffer
	);
	if (ret == NULL)
		free(buffer);
	return ret;
}


struct bundle *bundle6_generate_status_report(
	const struct bundle * const bundle,
	const struct bundle_status_report *report,
	const char *local_eid)
{
	return generate_record(
		bundle,
		local_eid,
		bundle->report_to,
		true,
		(uint8_t)(report->status),
		(uint8_t)(report->reason)
	);
}


struct bundle *bundle6_generate_custody_signal(
	const struct bundle * const bundle,
	const struct bundle_custody_signal *signal,
	const char *local_eid)
{
	return generate_record(
		bundle,
		local_eid,
		bundle->current_custodian,
		false,
		((uint8_t)(signal->reason) & 0x7F)
			| ((signal->type == BUNDLE_CS_TYPE_ACCEPTANCE)
				? 0x80 : 0x00),
		0
	);
}


// ---------------------------------------
// Administrative Record Parser (RFC 5050)
// ---------------------------------------

struct record_parser {
	enum parser_status status;
	enum record_parser_stage {
		RP_EXPECT_TYPE,
		RP_EXPECT_REPORT_FLAGS,
		RP_EXPECT_REPORT_REASON,
		RP_EXPECT_CUSTODY_STATUS,
		RP_EXPECT_FRAGMENT_OFFSET,
		RP_EXPECT_FRAGMENT_LENGTH,
		RP_EXPECT_RECORD_DTN_SECONDS,
		RP_EXPECT_RECORD_DTN_NANOSECONDS,
		RP_EXPECT_BUNDLE_CREATION_TIMESTAMP,
		RP_EXPECT_BUNDLE_CREATION_SEQUENCE,
		RP_EXPECT_BUNDLE_SOURCE_LENGTH,
		RP_EXPECT_BUNDLE_SOURCE_EID
	} stage;
	struct sdnv_state sdnv_state;
	struct bundle_administrative_record *record;
	uint16_t current_index, bytes_remaining;
};

static enum ud3tn_result record_parser_init(struct record_parser *parser)
{
	parser->status = PARSER_STATUS_GOOD;
	parser->stage = RP_EXPECT_TYPE;
	parser->record = malloc(sizeof(struct bundle_administrative_record));
	if (parser->record == NULL)
		return UD3TN_FAIL;
	parser->record->status_report = NULL;
	parser->record->custody_signal = NULL;
	parser->record->fragment_offset = 0;
	parser->record->fragment_length = 0;
	parser->record->event_timestamp = 0;
	parser->record->event_nanoseconds = 0;
	parser->record->bundle_creation_timestamp_ms = 0;
	parser->record->bundle_sequence_number = 0;
	parser->record->bundle_source_eid_length = 0;
	parser->record->bundle_source_eid = NULL;
	return UD3TN_OK;
}

static void record_parser_next(
	struct record_parser *parser, enum record_parser_stage next)
{
	parser->stage = next;
	sdnv_reset(&parser->sdnv_state);
}

static int record_parser_wait_for_sdnv(
	struct record_parser *parser, enum record_parser_stage next)
{
	switch (parser->sdnv_state.status) {
	case SDNV_IN_PROGRESS:
		break;
	case SDNV_DONE:
		record_parser_next(parser, next);
		return 1;
	case SDNV_ERROR:
		parser->status = PARSER_STATUS_ERROR;
		break;
	}
	return 0;
}

static void record_parser_read_byte(struct record_parser *parser, uint8_t byte)
{
	switch (parser->stage) {
	case RP_EXPECT_TYPE:
		parser->record->type = (byte >> 4) & 0x0F;
		parser->record->flags = byte & 0x0F;
		if (parser->record->type == BUNDLE_AR_STATUS_REPORT) {
			parser->record->status_report =
				(struct bundle_status_report *)malloc(
				sizeof(struct bundle_status_report));
			record_parser_next(
				parser, RP_EXPECT_REPORT_FLAGS);
		} else if (parser->record->type
			== BUNDLE_AR_CUSTODY_SIGNAL
		) {
			parser->record->custody_signal =
				(struct bundle_custody_signal *)malloc(
				sizeof(struct bundle_custody_signal));
			record_parser_next(
				parser, RP_EXPECT_CUSTODY_STATUS);
		} else {
			/* Can't parse other types */
			parser->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_REPORT_FLAGS:
		parser->record->status_report->status = byte;
		record_parser_next(parser, RP_EXPECT_REPORT_REASON);
		break;
	case RP_EXPECT_REPORT_REASON:
		parser->record->status_report->reason = byte;
		if (HAS_FLAG(parser->record->flags,
			BUNDLE_AR_FLAG_FRAGMENT)
		) {
			record_parser_next(parser,
				RP_EXPECT_FRAGMENT_OFFSET);
		} else {
			record_parser_next(parser,
				RP_EXPECT_RECORD_DTN_SECONDS);
		}
		break;
	case RP_EXPECT_CUSTODY_STATUS:
		parser->record->custody_signal->type = (byte >> 7)
			? BUNDLE_CS_TYPE_ACCEPTANCE
			: BUNDLE_CS_TYPE_REFUSAL;
		parser->record->custody_signal->reason = byte & 0x7F;
		if (HAS_FLAG(parser->record->flags,
				BUNDLE_AR_FLAG_FRAGMENT)
		) {
			record_parser_next(parser,
				RP_EXPECT_FRAGMENT_OFFSET);
		} else {
			record_parser_next(parser,
				RP_EXPECT_RECORD_DTN_SECONDS);
		}
		break;
	case RP_EXPECT_FRAGMENT_OFFSET:
		sdnv_read_u32(&parser->sdnv_state,
			&parser->record->fragment_offset, byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_FRAGMENT_LENGTH);
		break;
	case RP_EXPECT_FRAGMENT_LENGTH:
		sdnv_read_u32(&parser->sdnv_state,
			&parser->record->fragment_length, byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_RECORD_DTN_SECONDS);
		break;
	case RP_EXPECT_RECORD_DTN_SECONDS:
		sdnv_read_u64(&parser->sdnv_state,
			&parser->record->event_timestamp, byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_RECORD_DTN_NANOSECONDS);
		break;
	case RP_EXPECT_RECORD_DTN_NANOSECONDS:
		sdnv_read_u32(&parser->sdnv_state,
			&parser->record->event_nanoseconds, byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_BUNDLE_CREATION_TIMESTAMP);
		break;
	case RP_EXPECT_BUNDLE_CREATION_TIMESTAMP:
		sdnv_read_u64(&parser->sdnv_state,
			&parser->record->bundle_creation_timestamp_ms,
			byte);
		// Parse the current byte and apply time conversion to ms if it
		// is the last byte
		if (record_parser_wait_for_sdnv(
			parser, RP_EXPECT_BUNDLE_CREATION_SEQUENCE)
		)
			parser->record->bundle_creation_timestamp_ms *= 1000;
		break;
	case RP_EXPECT_BUNDLE_CREATION_SEQUENCE:
		sdnv_read_u64(&parser->sdnv_state,
			&parser->record->bundle_sequence_number,
			byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_BUNDLE_SOURCE_LENGTH);
		break;
	case RP_EXPECT_BUNDLE_SOURCE_LENGTH:
		sdnv_read_u16(&parser->sdnv_state,
			&parser->record->bundle_source_eid_length,
			byte);
		if (record_parser_wait_for_sdnv(
			parser, RP_EXPECT_BUNDLE_SOURCE_EID)
		) {
			parser->record->bundle_source_eid =
				malloc(parser->record
					->bundle_source_eid_length + 1);
			if (!parser->record->bundle_source_eid)
				parser->status = PARSER_STATUS_ERROR;
			parser->current_index = 0;
			parser->bytes_remaining = parser->record
					->bundle_source_eid_length;
		}
		break;
	case RP_EXPECT_BUNDLE_SOURCE_EID:
		parser->bytes_remaining--;
		parser->record->bundle_source_eid
			[parser->current_index++] = (char)byte;
		if (parser->bytes_remaining == 0) {
			parser->record->bundle_source_eid
			[parser->current_index] = '\0';
			parser->status = PARSER_STATUS_DONE;
		}
		fallthrough;
	default:
		parser->status = PARSER_STATUS_ERROR;
		break;
	}
}

struct bundle_administrative_record *bundle6_parse_administrative_record(
	const uint8_t *const data, const size_t length)
{
	struct record_parser parser;
	uint32_t i = 0;
	const uint8_t *cur_byte;

	ASSERT(data != NULL);
	if (record_parser_init(&parser) != UD3TN_OK)
		return NULL;
	cur_byte = data;
	while (parser.status == PARSER_STATUS_GOOD && i < length) {
		record_parser_read_byte(&parser, *cur_byte);
		i++;
		cur_byte++;
	}
	if (parser.status == PARSER_STATUS_DONE)
		return parser.record;
	free_administrative_record(parser.record);
	return NULL;
}
