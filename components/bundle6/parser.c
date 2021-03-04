#include "bundle6/parser.h"

#include "ud3tn/bundle_storage_manager.h"
#include "ud3tn/config.h"
#include "ud3tn/common.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


struct parser *bundle6_parser_init(
	struct bundle6_parser *state,
	void (*send_callback)(struct bundle *, void *), void *param)
{
	state->basedata = malloc(sizeof(struct parser));
	if (state->basedata == NULL)
		return NULL;
	state->send_callback = send_callback;
	state->send_param = param;
	state->bundle = NULL;
	state->dict = NULL;
	state->basedata->status = PARSER_STATUS_ERROR;
	if (bundle6_parser_reset(state) != UD3TN_OK)
		return NULL;
	return state->basedata;
}

enum ud3tn_result bundle6_parser_reset(struct bundle6_parser *state)
{
	if (state->basedata->status == PARSER_STATUS_GOOD &&
			state->current_stage == PARSER_STAGE_VERSION)
		return UD3TN_OK;

	state->basedata->status = PARSER_STATUS_GOOD;
	state->basedata->flags = PARSER_FLAG_NONE;
	state->error = PARSER_ERROR_NONE;

	state->current_stage = PARSER_STAGE_VERSION;
	state->next_stage    = PARSER_STAGE_VERSION;
	state->current_block = PARSER_BLOCK_PRIMARY;

	sdnv_reset(&state->sdnv_state);

	state->primary_bytes_remaining = 0;
	state->cur_bytes_remaining = 0;
	state->current_index = 0;
	state->current_size = bundle_storage_get_usage()
		+ sizeof(struct bundle);
	state->last_block = 0;

	if (state->bundle != NULL)
		bundle_reset(state->bundle);
	else
		state->bundle = bundle_init();
	if (state->bundle == NULL)
		return UD3TN_FAIL;

	if (state->dict != NULL) {
		free(state->dict);
		state->dict = NULL;
	}

	state->current_block_entry = &state->bundle->blocks;

	return UD3TN_OK;
}

enum ud3tn_result bundle6_parser_deinit(struct bundle6_parser *state)
{
	free(state->basedata);
	if (state->bundle != NULL)
		bundle_free(state->bundle);
	if (state->dict != NULL)
		free(state->dict);

	return UD3TN_OK;
}

static inline bool bundle6_parser_length_known(struct bundle6_parser *state)
{
	return state->current_stage > PARSER_STAGE_BLOCK_LENGTH;
}

static inline bool bundle6_parser_data_done(struct bundle6_parser *state)
{
	return state->cur_bytes_remaining == 0;
}

static inline bool bundle6_parser_is_last_block(struct bundle6_parser *state)
{
	return state->last_block;
}

static inline void bundle6_parser_begin_block(
	struct bundle6_parser *state, uint8_t type)
{
	struct bundle_block *new = bundle_block_create(type);

	if (new == NULL) {
		state->basedata->status = PARSER_STATUS_ERROR;
		return;
	}
	if (type == BUNDLE_BLOCK_TYPE_PAYLOAD) {
		state->bundle->payload_block = new;
		state->current_block = PARSER_BLOCK_PAYLOAD;
	} else {
		state->current_block = PARSER_BLOCK_EXTENSION;
	}
	*state->current_block_entry = bundle_block_entry_create(new);
	if (*state->current_block_entry == NULL) {
		bundle_block_free(new);
		state->basedata->status = PARSER_STATUS_ERROR;
		return;
	}
}

static inline void bundle6_parser_end_block(struct bundle6_parser *state)
{
	state->current_block_entry = &(*state->current_block_entry)->next;
}

static char *bundle6_read_eid(const char *dict, const size_t dict_len,
			      struct bundle6_eid_reference ref)
{
	if (ref.scheme_offset >= dict_len || ref.ssp_offset >= dict_len)
		return NULL;

	size_t scheme_len = strlen(&dict[ref.scheme_offset]);
	size_t ssp_len = strlen(&dict[ref.ssp_offset]);
	char *eid = malloc(scheme_len + ssp_len + 2);

	if (eid == NULL)
		return NULL;
	if (ref.scheme_offset + scheme_len > dict_len)
		goto fail;
	if (strchr(&dict[ref.scheme_offset], ':') != NULL)
		goto fail; // Scheme may not contain colons
	memcpy(eid, &dict[ref.scheme_offset], scheme_len);
	eid[scheme_len] = ':';
	if (ref.ssp_offset + ssp_len > dict_len)
		goto fail;
	memcpy(&eid[scheme_len + 1], &dict[ref.ssp_offset], ssp_len);
	eid[scheme_len + ssp_len + 1] = 0;

	return eid;
fail:
	free(eid);
	return NULL;
}

static bool bundle_is_valid(struct bundle *const bundle)
{
	return bundle->payload_block != NULL;
}

static inline void bundle6_parser_send_bundle(struct bundle6_parser *state)
{
	struct bundle *ptr = state->bundle;

	state->bundle = NULL;
	if (state->send_callback == NULL || !bundle_is_valid(ptr))
		bundle_free(ptr);
	else
		state->send_callback(ptr, state->send_param);
}

static inline void bundle6_parser_next(struct bundle6_parser *state)
{
	switch (state->next_stage) {
	case PARSER_STAGE_PROC_FLAGS:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_BLOCK_LENGTH:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_DESTINATION_EID_SCHEME:
		state->primary_bytes_remaining =
			state->bundle->primary_block_length;
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_DESTINATION_EID_SSP:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_SOURCE_EID_SCHEME:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_SOURCE_EID_SSP:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_REPORT_EID_SCHEME:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_REPORT_EID_SSP:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_CUSTODIAN_EID_SCHEME:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_CUSTODIAN_EID_SSP:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_TIMESTAMP:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_SEQUENCE_NUM:
		state->bundle->creation_timestamp_ms *= 1000; // s -> ms
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_LIFETIME:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_DICT_LENGTH:
		state->bundle->lifetime_ms *= 1000; // s -> ms
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_DICTIONARY:
		state->current_index = 0;
		if (state->dict_length == 0) {
			state->basedata->status = PARSER_STATUS_ERROR;
			break;
		}
		state->cur_bytes_remaining = state->dict_length;
		state->current_size += state->cur_bytes_remaining + 1;
		if (state->current_size > BUNDLE_QUOTA) {
			state->basedata->status = PARSER_STATUS_ERROR;
		} else {
			// We ensure that the dict is zero-terminated
			state->dict = malloc(state->cur_bytes_remaining + 1);
			if (state->dict == NULL)
				state->basedata->status
					= PARSER_STATUS_ERROR;
			else
				state->dict[state->cur_bytes_remaining] = 0;
		}
		break;
	case PARSER_STAGE_FRAGMENT_OFFSET:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_ADU_LENGTH:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_BLOCK_TYPE:
		state->current_block = PARSER_BLOCK_GENERIC;
		break;
	case PARSER_STAGE_BLOCK_FLAGS:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_BLOCK_EID_REF_CNT:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_BLOCK_EID_REF_SCH:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_BLOCK_EID_REF_SSP:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_BLOCK_DATA_LENGTH:
		sdnv_reset(&state->sdnv_state);
		break;
	case PARSER_STAGE_BLOCK_DATA:
		state->current_index = 0;
		state->cur_bytes_remaining = 0;
		state->basedata->next_bytes =
			(*state->current_block_entry)->data->length;
		state->current_size += state->basedata->next_bytes;
		if (state->current_size > BUNDLE_QUOTA) {
			state->basedata->status = PARSER_STATUS_ERROR;
		} else {
			state->basedata->next_buffer =
				malloc(state->basedata->next_bytes);
			if (state->basedata->next_buffer == NULL) {
				state->basedata->status = PARSER_STATUS_ERROR;
			} else {
				(*state->current_block_entry)->data->data =
					state->basedata->next_buffer;
				state->basedata->flags |= PARSER_FLAG_BULK_READ;
			}
		}
		break;
	default:
		break;
	}

	state->current_stage = state->next_stage;

	if (state->current_stage == PARSER_STAGE_DONE) {
		state->basedata->status = PARSER_STATUS_DONE;
		bundle6_parser_send_bundle(state);
	}
}

/**
 * Checks if the SDNV parser is done or has encountered an error
 *
 * In the former case, sets the parser to continue with the next stage,
 * in the latter case, "forwards" the error to the parser.
 */
static inline int bundle6_parser_wait_for_sdnv(
	struct bundle6_parser *state, enum bundle6_parser_stage next_stage)
{
	switch (state->sdnv_state.status) {
	case SDNV_IN_PROGRESS:
		/* intentional, do nothing - unless
		 * a termination state is reached
		 */
		break;
	case SDNV_DONE:
		state->next_stage = next_stage;
		return 1;
	case SDNV_ERROR:
		state->basedata->status = PARSER_STATUS_ERROR;
		state->error  = PARSER_ERROR_SDNV_FAILURE;
		break;
	}
	return 0;
}

static void bundle6_parser_read_byte(struct bundle6_parser *state,
	uint8_t byte)
{
	if (state->current_block == PARSER_BLOCK_PRIMARY) {
		if (bundle6_parser_length_known(state)
			&& (state->primary_bytes_remaining == 0)
		) {
			state->basedata->status = PARSER_STATUS_ERROR;
			state->error = PARSER_ERROR_BLOCK_LENGTH_EXHAUSTED;
			return;
		}
		state->primary_bytes_remaining--;
	}

	switch (state->current_stage) {
	case PARSER_STAGE_VERSION:
		state->bundle->protocol_version = byte;
		state->next_stage = PARSER_STAGE_PROC_FLAGS;
		break;
	case PARSER_STAGE_PROC_FLAGS:
		ASSERT(sizeof(state->bundle->proc_flags) == sizeof(uint32_t));
		sdnv_read_u32(&state->sdnv_state,
			(uint32_t *)&state->bundle->proc_flags, byte);
		bundle6_parser_wait_for_sdnv(
				state, PARSER_STAGE_BLOCK_LENGTH);
		break;
	case PARSER_STAGE_BLOCK_LENGTH:
		sdnv_read_u16(&state->sdnv_state,
				&state->bundle->primary_block_length, byte);
			bundle6_parser_wait_for_sdnv(
				state, PARSER_STAGE_DESTINATION_EID_SCHEME);
		break;
	case PARSER_STAGE_DESTINATION_EID_SCHEME:
		sdnv_read_u16(&state->sdnv_state,
			      &state->destination_eidref.scheme_offset, byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_DESTINATION_EID_SSP);
		break;
	case PARSER_STAGE_DESTINATION_EID_SSP:
		sdnv_read_u16(&state->sdnv_state,
			      &state->destination_eidref.ssp_offset, byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_SOURCE_EID_SCHEME);
		break;
	case PARSER_STAGE_SOURCE_EID_SCHEME:
		sdnv_read_u16(&state->sdnv_state,
			      &state->source_eidref.scheme_offset, byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_SOURCE_EID_SSP);
		break;
	case PARSER_STAGE_SOURCE_EID_SSP:
		sdnv_read_u16(&state->sdnv_state,
			      &state->source_eidref.ssp_offset, byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_REPORT_EID_SCHEME);
		break;
	case PARSER_STAGE_REPORT_EID_SCHEME:
		sdnv_read_u16(&state->sdnv_state,
			      &state->report_to_eidref.scheme_offset, byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_REPORT_EID_SSP);
		break;
	case PARSER_STAGE_REPORT_EID_SSP:
		sdnv_read_u16(&state->sdnv_state,
			      &state->report_to_eidref.ssp_offset, byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_CUSTODIAN_EID_SCHEME);
		break;
	case PARSER_STAGE_CUSTODIAN_EID_SCHEME:
		sdnv_read_u16(&state->sdnv_state,
			      &state->custodian_eidref.scheme_offset, byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_CUSTODIAN_EID_SSP);
		break;
	case PARSER_STAGE_CUSTODIAN_EID_SSP:
		sdnv_read_u16(&state->sdnv_state,
			      &state->custodian_eidref.ssp_offset, byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_TIMESTAMP);
		break;
	case PARSER_STAGE_TIMESTAMP:
		sdnv_read_u64(
			&state->sdnv_state,
			&state->bundle->creation_timestamp_ms,
			byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_SEQUENCE_NUM);
		break;
	case PARSER_STAGE_SEQUENCE_NUM:
		sdnv_read_u64(
			&state->sdnv_state,
			&state->bundle->sequence_number,
			byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_LIFETIME);
		break;
	case PARSER_STAGE_LIFETIME:
		sdnv_read_u64(
			&state->sdnv_state,
			&state->bundle->lifetime_ms,
			byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_DICT_LENGTH);
		break;
	case PARSER_STAGE_DICT_LENGTH:
		sdnv_read_u16(&state->sdnv_state, &state->dict_length,
			      byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_DICTIONARY);
		break;
	case PARSER_STAGE_DICTIONARY:
		--state->cur_bytes_remaining;
		((uint8_t *)state->dict)[state->current_index++] = byte;
		if (bundle6_parser_data_done(state)) {
			((uint8_t *)state->dict)[state->current_index++] = 0;

			// Allocate EID references
			//
			// source
			state->bundle->source = bundle6_read_eid(
				state->dict, state->dict_length,
				state->source_eidref
			);
			// destination
			state->bundle->destination = bundle6_read_eid(
				state->dict, state->dict_length,
				state->destination_eidref
			);
			// report-to
			state->bundle->report_to = bundle6_read_eid(
				state->dict, state->dict_length,
				state->report_to_eidref
			);
			// custodian
			state->bundle->current_custodian = bundle6_read_eid(
				state->dict, state->dict_length,
				state->custodian_eidref
			);
			if (state->bundle->source == NULL ||
					state->bundle->destination == NULL ||
					state->bundle->report_to == NULL ||
					state->bundle->current_custodian == NULL
			) {
				state->basedata->status = PARSER_STATUS_ERROR;
				break;
			}

			if (bundle_is_fragmented(state->bundle))
				state->next_stage =
					PARSER_STAGE_FRAGMENT_OFFSET;
			else
				state->next_stage =
					PARSER_STAGE_BLOCK_TYPE;
		}
		break;
	case PARSER_STAGE_FRAGMENT_OFFSET:
		sdnv_read_u32(
			&state->sdnv_state,
			&state->bundle->fragment_offset,
			byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_ADU_LENGTH);
		break;
	case PARSER_STAGE_ADU_LENGTH:
		sdnv_read_u32(
			&state->sdnv_state,
			&state->bundle->total_adu_length,
			byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_BLOCK_TYPE);
		break;
	case PARSER_STAGE_BLOCK_TYPE:
		state->next_stage = PARSER_STAGE_BLOCK_FLAGS;
		bundle6_parser_begin_block(state, byte);
		break;
	case PARSER_STAGE_BLOCK_FLAGS:
		sdnv_read_u8(
			&state->sdnv_state,
			(uint8_t *)&(*state->current_block_entry)
				->data->flags, byte);
		if (bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_BLOCK_DATA_LENGTH)) {
			enum bundle_block_flags flags =
				(*state->current_block_entry)
					->data->flags;
			state->last_block = HAS_FLAG(flags,
				BUNDLE_V6_BLOCK_FLAG_LAST_BLOCK);
			if (HAS_FLAG(flags,
				BUNDLE_V6_BLOCK_FLAG_HAS_EID_REF_FIELD)
			) {
				state->next_stage =
					PARSER_STAGE_BLOCK_EID_REF_CNT;
			}
		}
		break;
	case PARSER_STAGE_BLOCK_EID_REF_CNT:
		sdnv_read_u32(&state->sdnv_state, &state->current_size, byte);
		if (bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_BLOCK_EID_REF_SCH)) {
			state->current_index = 0;
			if (state->current_size == 0) {
				state->next_stage
					= PARSER_STAGE_BLOCK_DATA_LENGTH;
			}
		}
		break;
	case PARSER_STAGE_BLOCK_EID_REF_SCH:
		sdnv_read_u16(&state->sdnv_state,
			      &state->cur_eidref.scheme_offset, byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_BLOCK_EID_REF_SSP);
		break;
	case PARSER_STAGE_BLOCK_EID_REF_SSP:
		sdnv_read_u16(&state->sdnv_state,
			      &state->cur_eidref.ssp_offset, byte);
		if (bundle6_parser_wait_for_sdnv(
				state, PARSER_STAGE_BLOCK_EID_REF_SCH)) {
			struct endpoint_list *entry = malloc(
				sizeof(struct endpoint_list));
			if (entry == NULL) {
				state->basedata->status = PARSER_STATUS_ERROR;
				break;
			}

			entry->eid = bundle6_read_eid(
				state->dict, state->dict_length,
				state->cur_eidref);
			entry->next = (*state->current_block_entry)
				->data->eid_refs;
			(*state->current_block_entry)
				->data->eid_refs = entry;
			state->current_index++;

			if (entry->eid == NULL)
				state->basedata->status = PARSER_STATUS_ERROR;
			else if (state->current_index == state->current_size)
				state->next_stage =
					PARSER_STAGE_BLOCK_DATA_LENGTH;
		}
		break;
	case PARSER_STAGE_BLOCK_DATA_LENGTH:
		sdnv_read_u32(
			&state->sdnv_state,
			&(*state->current_block_entry)
				->data->length,
			byte);
		bundle6_parser_wait_for_sdnv(
			state, PARSER_STAGE_BLOCK_DATA);
		break;
	case PARSER_STAGE_BLOCK_DATA:
		if (state->cur_bytes_remaining) {
			state->basedata->status = PARSER_STATUS_ERROR;
		} else {
			/*
			 * We do not parse byte as it is, only passed for
			 * triggering this logic and always has the value zero.
			 * See also inputProcessor.c
			 */
			bundle6_parser_end_block(state);
			if (bundle6_parser_is_last_block(state))
				state->next_stage = PARSER_STAGE_DONE;
			else
				state->next_stage = PARSER_STAGE_BLOCK_TYPE;
		}
		break;
	default:
		state->basedata->status = PARSER_STATUS_ERROR;
		break;
	}

	if (state->current_stage != state->next_stage &&
	    state->basedata->status != PARSER_STATUS_ERROR)
		bundle6_parser_next(state);
}

size_t bundle6_parser_read(struct bundle6_parser *parser,
	const uint8_t *buffer, size_t length)
{
	/*
	 * Special case:
	 *     Bulk read operation was performed.
	 *     This function is called with an empty buffer.
	 */
	if (buffer == NULL) {
		/* Feed zero byte */
		bundle6_parser_read_byte(parser, '\0');
		return 0;
	}

	size_t i = 0;

	while (i < length && parser->basedata->status == PARSER_STATUS_GOOD) {
		if (HAS_FLAG(parser->basedata->flags, PARSER_FLAG_BULK_READ)) {
			/*
			 * Input buffer does not contain enough data,
			 * let the outer function handle the bulk read
			 * operation
			 */
			if (i + parser->basedata->next_bytes > length)
				break;

			/* Perform bulk read operation directly */
			memcpy(parser->basedata->next_buffer,
				buffer + i,
				parser->basedata->next_bytes);

			/* Disables bulk read mode again */
			parser->basedata->flags &= ~PARSER_FLAG_BULK_READ;

			/* Feed zero byte */
			bundle6_parser_read_byte(parser, '\0');

			i += parser->basedata->next_bytes;
		} else {
			bundle6_parser_read_byte(parser, buffer[i]);
			i++;
		}
	}

	return i;
}
