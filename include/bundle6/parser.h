#ifndef BUNDLE6_PARSER_H_INCLUDED
#define BUNDLE6_PARSER_H_INCLUDED

#include "bundle6/bundle6.h"
#include "bundle6/sdnv.h"

#include "ud3tn/bundle.h"
#include "ud3tn/parser.h"
#include "ud3tn/result.h"

#include <stdint.h>

/**
 * Represents the part of the bundle the parser is currently processing
 * This serves to determine how incoming bytes are treated (and possibly stored)
 */
enum bundle6_parser_stage {
	PARSER_STAGE_VERSION,
	PARSER_STAGE_PROC_FLAGS,
	PARSER_STAGE_BLOCK_LENGTH,
	PARSER_STAGE_DESTINATION_EID_SCHEME,
	PARSER_STAGE_DESTINATION_EID_SSP,
	PARSER_STAGE_SOURCE_EID_SCHEME,
	PARSER_STAGE_SOURCE_EID_SSP,
	PARSER_STAGE_REPORT_EID_SCHEME,
	PARSER_STAGE_REPORT_EID_SSP,
	PARSER_STAGE_CUSTODIAN_EID_SCHEME,
	PARSER_STAGE_CUSTODIAN_EID_SSP,
	PARSER_STAGE_TIMESTAMP,
	PARSER_STAGE_SEQUENCE_NUM,
	PARSER_STAGE_LIFETIME,
	PARSER_STAGE_DICT_LENGTH,
	PARSER_STAGE_DICTIONARY,
	PARSER_STAGE_FRAGMENT_OFFSET,
	PARSER_STAGE_ADU_LENGTH,
	PARSER_STAGE_BLOCK_TYPE,
	PARSER_STAGE_BLOCK_FLAGS,
	PARSER_STAGE_BLOCK_EID_REF_CNT,
	PARSER_STAGE_BLOCK_EID_REF_SCH,
	PARSER_STAGE_BLOCK_EID_REF_SSP,
	PARSER_STAGE_BLOCK_DATA_LENGTH,
	PARSER_STAGE_BLOCK_DATA,
	PARSER_STAGE_DONE
};

/**
 * Specifies the currently parsed block
 */
enum bundle6_parser_block {
	PARSER_BLOCK_PRIMARY,
	PARSER_BLOCK_GENERIC,
	PARSER_BLOCK_PAYLOAD,
	PARSER_BLOCK_EXTENSION
};

/**
 * Detailed cause of an error, if one occured (see `enum parser_status`)
 */
enum bundle6_parser_error {
	PARSER_ERROR_NONE,
	PARSER_ERROR_BLOCK_LENGTH_EXHAUSTED,
	PARSER_ERROR_SDNV_FAILURE,
};

/**
 * Represents the parser state
 * if you add or change something here, please don't forget to update
 * parser_reset(), too!
 */
struct bundle6_parser {
	struct parser *basedata;
	void (*send_callback)(struct bundle *, void *);
	void *send_param;

	enum bundle6_parser_error error;

	enum bundle6_parser_stage current_stage;
	enum bundle6_parser_stage next_stage;
	enum bundle6_parser_block current_block;

	struct sdnv_state sdnv_state;
	struct bundle *bundle;

	uint16_t primary_bytes_remaining;
	uint32_t cur_bytes_remaining;
	uint32_t current_index;
	uint32_t current_size;

	struct bundle6_eid_reference destination_eidref;
	struct bundle6_eid_reference source_eidref;
	struct bundle6_eid_reference report_to_eidref;
	struct bundle6_eid_reference custodian_eidref;
	struct bundle6_eid_reference cur_eidref;

	uint16_t dict_length;
	char *dict;

	struct bundle_block_list **current_block_entry;
	uint8_t last_block;
};

struct parser *bundle6_parser_init(
	struct bundle6_parser *state,
	void (*send_callback)(struct bundle *, void *), void *param);
enum ud3tn_result bundle6_parser_reset(struct bundle6_parser *state);
enum ud3tn_result bundle6_parser_deinit(struct bundle6_parser *state);

size_t bundle6_parser_read(struct bundle6_parser *state,
	const uint8_t *buffer, size_t length);

#endif /* BUNDLE6_PARSER_H_INCLUDED */
