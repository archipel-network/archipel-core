#ifndef SPP_PARSER_H
#define SPP_PARSER_H

#include "spp/spp_internal.h"

#include "ud3tn/parser.h"

#include <stddef.h>
#include <stdint.h>

enum spp_parser_state {
	SPP_PARSER_STATE_PH_P1_MSB = 0,
	SPP_PARSER_STATE_PH_P1_LSB = 1,
	SPP_PARSER_STATE_PH_P2_MSB = 2,
	SPP_PARSER_STATE_PH_P2_LSB = 3,
	SPP_PARSER_STATE_PH_LEN_MSB = 4,
	SPP_PARSER_STATE_PH_LEN_LSB = 5,
	SPP_PARSER_STATE_SH_TIMECODE_SUBPARSER = 6,
	SPP_PARSER_STATE_SH_ANCILLARY_SUBPARSER = 7,
	SPP_PARSER_STATE_DATA_SUBPARSER = 8,
};

struct spp_parser {
	struct parser base;
	uint16_t bufw;
	struct spp_primary_header_t header;
	const struct spp_context_t *ctx;
	enum spp_parser_state state;
	struct spp_tc_parser_t tc_parser;
	size_t data_length;
	uint64_t dtn_timestamp;
};

struct parser *spp_parser_init(struct spp_parser *parser,
			       const struct spp_context_t *ctx);
size_t spp_parser_read(struct spp_parser *parser,
	const uint8_t *buffer, size_t length);
bool spp_parser_get_meta(const struct spp_parser *parser,
			 struct spp_meta_t *dest);
bool spp_parser_get_data_length(const struct spp_parser *parser,
			 size_t *dest);
void spp_parser_reset(struct spp_parser *parser);

#endif // SPP_PARSER_H
