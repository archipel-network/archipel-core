#include "spp/spp_parser.h"
#include "spp/spp_internal.h"

#include "ud3tn/parser.h"

#include <assert.h>


struct parser *spp_parser_init(struct spp_parser *parser,
			       const struct spp_context_t *ctx)
{
	parser->ctx = ctx;
	spp_parser_reset(parser);
	return &parser->base;
}

bool spp_parse_byte(struct spp_parser *parser,
		    const uint8_t byte)
{
	switch (parser->state) {
	case SPP_PARSER_STATE_PH_P1_MSB:
	{
		if (!spp_check_first_byte(byte)) {
			spp_parser_reset(parser);
			parser->base.status = PARSER_STATUS_ERROR;
			// ?!
			return true;
		}
		parser->bufw = byte << 8;
		parser->state = SPP_PARSER_STATE_PH_P1_LSB;
		return true;
	}
	case SPP_PARSER_STATE_PH_P1_LSB:
	{
		parser->bufw |= byte;
		spp_parse_ph_p1(parser->bufw, &parser->header);
		parser->state = SPP_PARSER_STATE_PH_P2_MSB;
		return true;
	}
	case SPP_PARSER_STATE_PH_P2_MSB:
	{
		parser->bufw = byte << 8;
		parser->state = SPP_PARSER_STATE_PH_P2_LSB;
		return true;
	}
	case SPP_PARSER_STATE_PH_P2_LSB:
	{
		parser->bufw |= byte;
		spp_parse_ph_p2(parser->bufw, &parser->header);
		parser->state = SPP_PARSER_STATE_PH_LEN_MSB;
		return true;
	}
	case SPP_PARSER_STATE_PH_LEN_MSB:
	{
		parser->bufw = byte << 8;
		parser->state = SPP_PARSER_STATE_PH_LEN_LSB;
		return true;
	}
	case SPP_PARSER_STATE_PH_LEN_LSB:
	{
		parser->bufw |= byte;
		spp_parse_ph_len(parser->bufw, &parser->header);

		parser->data_length = parser->header.data_length;
		parser->data_length -= parser->ctx->ancillary_data_len;

		if (!parser->header.has_secondary_header) {
			// TODO: init subparser if implemented here
			parser->state = SPP_PARSER_STATE_DATA_SUBPARSER;
			return false;
		}

		if (parser->ctx->timecode != NULL) {
			parser->state = SPP_PARSER_STATE_SH_TIMECODE_SUBPARSER;
			spp_tc_parser_init(parser->ctx->timecode,
					   &parser->tc_parser);
			break;
		}

		if (parser->ctx->ancillary_data_len > 0) {
			// TODO: init subparser if implemented here
			parser->state = SPP_PARSER_STATE_SH_ANCILLARY_SUBPARSER;
			break;
		}

		parser->state = SPP_PARSER_STATE_DATA_SUBPARSER;
		return false;
	}
	case SPP_PARSER_STATE_SH_TIMECODE_SUBPARSER:
	{
		parser->data_length -= 1;
		switch (spp_tc_parser_feed(&parser->tc_parser, byte)) {
		case SPP_TC_PARSER_GOOD:
		{
			return true;
		}
		case SPP_TC_PARSER_ERROR:
		{
			parser->base.status = PARSER_STATUS_ERROR;
			return false;
		}
		case SPP_TC_PARSER_DONE:
		{
			break;
		}
		}

		parser->dtn_timestamp = spp_tc_get_dtn_timestamp(
					&parser->tc_parser);

		if (parser->ctx->ancillary_data_len > 0) {
			// TODO: init subparser if implemented here
			parser->state = SPP_PARSER_STATE_SH_ANCILLARY_SUBPARSER;
			break;
		}

		parser->state = SPP_PARSER_STATE_DATA_SUBPARSER;
		return false;
	}
	case SPP_PARSER_STATE_SH_ANCILLARY_SUBPARSER:
	{
		// NOTE: Has to be implemented externally
		assert(false);
		break;
	}
	case SPP_PARSER_STATE_DATA_SUBPARSER:
	{
		// NOTE: Has to be implemented externally
		assert(false);
		break;
	}
	}

	return true;
}


size_t spp_parser_read(struct spp_parser *parser,
		       const uint8_t *buffer,
		       size_t length)
{
	const uint8_t *const end = buffer + length;
	const uint8_t *cur = buffer;

	for (; cur != end; ++cur) {
		if (!spp_parse_byte(parser, *cur)) {
			/* need to increase by one here to ensure that read
			 * count is correct.
			 */
			cur += 1;
			break;
		}
	}

	return cur - buffer;
}

void spp_parser_reset(struct spp_parser *parser)
{
	parser->base.status = PARSER_STATUS_GOOD;
	parser->base.next_bytes = 0;
	parser->base.flags = PARSER_FLAG_NONE;
	parser->state = SPP_PARSER_STATE_PH_P1_MSB;
}

bool spp_parser_get_meta(const struct spp_parser *parser,
			 struct spp_meta_t *dest)
{
	if (parser->state < SPP_PARSER_STATE_SH_ANCILLARY_SUBPARSER) {
		/* metadata is only ready after the timecode has been parsed
		 * as time is part of the metadata.
		 */
		return false;
	}

	dest->apid = parser->header.apid;
	dest->is_request = parser->header.is_request;
	dest->segment_number = parser->header.segment_number;
	dest->segment_status = parser->header.segment_status;
	dest->dtn_timestamp = parser->dtn_timestamp;
	dest->dtn_counter = 0;

	return true;
}

bool spp_parser_get_data_length(const struct spp_parser *parser,
				size_t *dest)
{
	if (parser->state < SPP_PARSER_STATE_SH_ANCILLARY_SUBPARSER) {
		/* metadata is only ready after the timecode has been parsed
		 * since timecode may be variable length, we need to parse it
		 * first to be sure when it’s over.
		 */
		return false;
	}

	*dest = parser->data_length;
	return true;
}
