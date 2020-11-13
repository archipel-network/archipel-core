#include "spp/spp_timecodes.h"

#include <string.h>

#define CCSDS_EPOCH_TO_DTN_EPOCH (1325376000ULL)

#define UNSEGMENTED_BASE_UNIT_LONGP_THRESHOLD (4)
#define UNSEGMENTED_FRACTIONAL_LONGP_THRESHOLD (3)

static int spp_tc_parser_start_from_config(struct spp_tc_parser_t *parser)
{
	// FIXME: set the state for the correct format
	parser->state.unsegmented.read_base_unit = false;
	parser->state.unsegmented.base_unit_count = 0;
	parser->state.unsegmented.base_unit_remaining =
			parser->format.unsegmented.base_unit_octets;
	parser->state.unsegmented.fractional_remaining =
			parser->format.unsegmented.fractional_octets;
	parser->state.unsegmented.fractional_count = 0;
	parser->state.unsegmented.fractional_ptr = 0;

	return SPP_TC_PARSER_GOOD;
}

static int spp_tc_pfield_parse_first(
		struct spp_tc_parser_preamble_state_t *state,
		struct spp_tc_config_t *dest,
		const uint8_t byte)
{
	const bool has_second = (byte & 0x80) != 0;
	enum spp_tc_type detected_type = (byte & 0x70) >> 4;

	state->has_second_octet = has_second;
	state->detected_type = detected_type;

	switch (state->detected_type) {
	case SPP_TC_UNSEGMENTED_CCSDS_EPOCH:
	case SPP_TC_UNSEGMENTED_CUSTOM_EPOCH:
	{
		// FIXME: handle custom epoch
		dest->unsegmented.base_unit_octets = ((byte & 0x0c) >> 2) + 1;
		dest->unsegmented.fractional_octets = (byte & 0x03);
		break;
	}
	default:
		return SPP_TC_PARSER_ERROR;
	}

	return SPP_TC_PARSER_GOOD;
}

static int spp_tc_pfield_parse_second(
		struct spp_tc_parser_preamble_state_t *state,
		struct spp_tc_config_t *dest,
		const uint8_t byte)
{
	const bool has_more = (byte & 0x80) != 0;

	state->has_second_octet = has_more;

	switch (state->detected_type) {
	case SPP_TC_UNSEGMENTED_CCSDS_EPOCH:
	case SPP_TC_UNSEGMENTED_CUSTOM_EPOCH:
	{
		/* yes, it is += and not a proper extension to the MSBs
		 */
		dest->unsegmented.base_unit_octets += ((byte & 0x60) >> 5);
		dest->unsegmented.fractional_octets += ((byte & 0x1c) >> 2);
		break;
	}
	default:
		return SPP_TC_PARSER_ERROR;
	}

	return SPP_TC_PARSER_GOOD;
}

static int spp_tc_pfield_feed(struct spp_tc_parser_t *parser,
			      const uint8_t byte)
{
	int result;

	if (!parser->state.preamble.has_second_octet) {
		// parse first octet
		result = spp_tc_pfield_parse_first(
					&parser->state.preamble,
					&parser->format,
					byte);
	} else {
		// parse subsequent octets
		result = spp_tc_pfield_parse_second(
					&parser->state.preamble,
					&parser->format,
					byte);
	}

	if (result != SPP_TC_PARSER_GOOD) {
		/* forward error condition */
		return result;
	}

	if (!parser->state.preamble.has_second_octet) {
		parser->format.type = parser->state.preamble.detected_type;
		spp_tc_parser_start_from_config(parser);
	}

	return result;
}


struct spp_tc_context_t *spp_timecode_create_none(void)
{
	return NULL;
}

int spp_tc_configure_from_preamble(struct spp_tc_context_t *ctx,
				    const uint8_t *preamble,
				    const size_t preamble_len)
{
	if (preamble_len < 1) {
		/* preamble must be at least one byte */
		return -1;
	}

	struct spp_tc_parser_preamble_state_t state;
	int status = spp_tc_pfield_parse_first(&state, &ctx->defaults,
					       preamble[0]);
	if (status != SPP_TC_PARSER_GOOD) {
		/* fail early */
		return -1;
	}

	for (size_t i = 1; i < preamble_len; ++i) {
		if (!state.has_second_octet) {
			/* no more octets -> break */
			// TODO: should we report this as error?
			break;
		}
		status = spp_tc_pfield_parse_second(&state, &ctx->defaults,
						    preamble[i]);
		if (status != SPP_TC_PARSER_GOOD) {
			/* forward failure */
			return -1;
		}
	}

	ctx->defaults.type = state.detected_type;

	return 0;
}


void spp_tc_parser_init(const struct spp_tc_context_t *ctx,
			struct spp_tc_parser_t *parser)
{
	parser->ctx = ctx;
	if (ctx->with_p_field) {
		parser->format.type = SPP_TC_UNKNOWN;
		parser->state.preamble.has_second_octet = false;
		parser->status = SPP_TC_PARSER_GOOD;
	} else {
		// load config
		memcpy(&parser->format, &ctx->defaults,
		       sizeof(struct spp_tc_config_t));
		parser->status = spp_tc_parser_start_from_config(parser);
	}
}

static void spp_tc_unsegmented_finalize(struct spp_tc_parser_t *parser)
{
	// TODO: convert fractional_buf to actual fixed-point count value
}

static int spp_tc_unsegmented_advance(struct spp_tc_parser_t *parser)
{
	if (!parser->state.unsegmented.read_base_unit) {
		if (!parser->state.unsegmented.base_unit_remaining) {
			/* base unit read completely, go on with fractional */
			parser->state.unsegmented.read_base_unit = true;
		}
	}

	// not using else since the above branch can change the value!
	if (parser->state.unsegmented.read_base_unit) {
		if (!parser->state.unsegmented.fractional_remaining) {
			/* compute fractional value */
			spp_tc_unsegmented_finalize(parser);
			return SPP_TC_PARSER_DONE;
		}
	}

	return SPP_TC_PARSER_GOOD;
}

static int _spp_tc_parser_feed(struct spp_tc_parser_t *parser,
			       const uint8_t byte)
{
	switch (parser->format.type) {
	case SPP_TC_UNKNOWN:
	{
		return spp_tc_pfield_feed(parser, byte);
	}
	case SPP_TC_UNSEGMENTED_CCSDS_EPOCH:
	{
		if (!parser->state.unsegmented.read_base_unit) {
			// parsing base unit
			parser->state.unsegmented.base_unit_remaining -= 1;
			parser->state.unsegmented.base_unit_count <<= 8;
			parser->state.unsegmented.base_unit_count |= byte;
			return spp_tc_unsegmented_advance(parser);
		}

		// parsing fractional part
		// not fully implemented yet (see _finalize)
		const int index = parser->state.unsegmented.fractional_ptr++;

		parser->state.unsegmented.fractional_buf[index] = byte;
		parser->state.unsegmented.fractional_remaining -= 1;
		return spp_tc_unsegmented_advance(parser);
	}
	default:
		return SPP_TC_PARSER_ERROR;
	}
}

int spp_tc_parser_feed(struct spp_tc_parser_t *parser,
		       const uint8_t byte)
{
	int result = _spp_tc_parser_feed(parser, byte);

	parser->status = result;
	return result;
}

uint64_t spp_tc_get_dtn_timestamp(const struct spp_tc_parser_t *parser)
{
	if (parser->status != SPP_TC_PARSER_DONE) {
		/* not done yet -> return error value */
		return UINT64_MAX;
	}

	switch (parser->format.type) {
	case SPP_TC_UNSEGMENTED_CCSDS_EPOCH:
	{
		if (parser->state.unsegmented.base_unit_count <
				CCSDS_EPOCH_TO_DTN_EPOCH) {
			/* value outside DTN timestamp range,
			 * return error value
			 */
			return UINT64_MAX;
		}
		return parser->state.unsegmented.base_unit_count -
				CCSDS_EPOCH_TO_DTN_EPOCH;
	}
	default:
		return UINT64_MAX;
	}
}

size_t spp_tc_get_size(const struct spp_tc_context_t *ctx)
{
	size_t sz = 0;

	switch (ctx->defaults.type) {
	case SPP_TC_UNSEGMENTED_CCSDS_EPOCH:
	case SPP_TC_UNSEGMENTED_CUSTOM_EPOCH:
	{
		const struct spp_tc_unsegmented_config_t *const format =
				&ctx->defaults.unsegmented;
		sz += format->base_unit_octets;
		sz += format->fractional_octets;

		if (ctx->with_p_field) {
			const bool base_unit_wide =
					format->base_unit_octets >
					UNSEGMENTED_BASE_UNIT_LONGP_THRESHOLD;
			const bool fractional_wide =
					format->fractional_octets >
					UNSEGMENTED_FRACTIONAL_LONGP_THRESHOLD;
			const bool need_second_byte =
					base_unit_wide || fractional_wide;
			sz += need_second_byte ? 2 : 1;
		}

		break;
	}
	default:
		return 0;
	}

	return sz;
}

static int spp_tc_serialize_preamble(const struct spp_tc_config_t *format,
				     uint8_t **out)
{
	switch (format->type) {
	case SPP_TC_UNSEGMENTED_CCSDS_EPOCH:
	case SPP_TC_UNSEGMENTED_CUSTOM_EPOCH:
	{
		const bool base_unit_wide =
				format->unsegmented.base_unit_octets
				> UNSEGMENTED_BASE_UNIT_LONGP_THRESHOLD;
		const bool fractional_wide =
				format->unsegmented.fractional_octets
				> UNSEGMENTED_FRACTIONAL_LONGP_THRESHOLD;
		const bool need_second_byte =
				base_unit_wide || fractional_wide;

		uint8_t byte = format->type << 4;

		if (need_second_byte) {
			/* set flag for follow-up byte */
			byte |= 0x80;
		}

		if (base_unit_wide) {
			/* set maximum value */
			byte |= 0x0c;
		} else {
			byte |= (format->unsegmented.base_unit_octets-1) << 2;
		}

		if (fractional_wide) {
			/* set maximum value */
			byte |= 0x03;
		} else {
			byte |= format->unsegmented.fractional_octets;
		}

		*(*out)++ = byte;

		if (!need_second_byte) {
			/* no more data to write, exit with success */
			return 0;
		}

		byte = 0;

		if (base_unit_wide) {
			/* fill in the remainder of the value */
			byte |= (format->unsegmented.base_unit_octets -
				 UNSEGMENTED_BASE_UNIT_LONGP_THRESHOLD) << 5;
		}

		if (fractional_wide) {
			/* fill in the remainder of the value */
			byte |= (format->unsegmented.fractional_octets -
				 UNSEGMENTED_FRACTIONAL_LONGP_THRESHOLD) << 2;
		}

		*(*out)++ = byte;

		return 0;
	}
	default:
		return -1;
	}
}

static int serialize_unsegmented(
		const struct spp_tc_unsegmented_config_t *config,
		const uint64_t seconds,
		const uint64_t fractional,
		uint8_t **out)
{
	/* we scan the `seconds` value from most-significant supported octet to
	 * the least-siginifcant octet. we do that by shifting it to the right
	 * so that the needed octet is the least-significant one (this allows
	 * us to mask it out and assign it directly to the uint8_t buffer).
	 * for this, we start with a shift which gets us the most-significant
	 * octet which can be serialized with the current settings and then
	 * decrease the shift value by 8 (one octet) on each iteration.
	 */
	unsigned int shift = (config->base_unit_octets - 1) * 8;

	for (uint8_t i = 0; i < config->base_unit_octets; ++i) {
		*(*out)++ = (seconds >> shift) & 0xff;
		shift -= 8;
	}

	/* we do the same as we did for the `seconds`, but this time for the
	 * fractional part. we start at the highest octet and work down to
	 * less significant bits.
	 */
	shift = 64 - 8;  /* bit width of fractional minus one octet */
	for (uint8_t i = 0; i < config->fractional_octets; ++i) {
		*(*out)++ = (fractional >> shift) & 0xff;
		shift -= 8;
	}

	return 0;
}

int spp_tc_serialize(const struct spp_tc_context_t *ctx,
		     const uint64_t dtn_timestamp,
		     const uint32_t dtn_counter,
		     uint8_t **out)
{
	if (ctx->with_p_field) {
		if (spp_tc_serialize_preamble(&ctx->defaults, out) != 0) {
			/* forward error condition */
			return -1;
		}
	}

	switch (ctx->defaults.type) {
	case SPP_TC_UNSEGMENTED_CCSDS_EPOCH:
	{
		const uint64_t seconds =
				dtn_timestamp + CCSDS_EPOCH_TO_DTN_EPOCH;
		const uint64_t fractional =
				(uint64_t)dtn_counter << 32;
		return serialize_unsegmented(&ctx->defaults.unsegmented,
					     seconds,
					     fractional,
					     out);
	}
	default:
		return -1;
	}

	return 0;
}
