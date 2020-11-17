#ifndef SPP_TIMECODES_H
#define SPP_TIMECODES_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#define SPP_TC_UNSEGMENTED_BASE_UNIT_MAX_OCTETS (4+3)
#define SPP_TC_UNSEGMENTED_FRACTIONAL_MAX_OCTETS (3+3)

enum spp_tc_parser_status {
	SPP_TC_PARSER_GOOD = 0,
	SPP_TC_PARSER_ERROR = -1,
	SPP_TC_PARSER_DONE = 1,
};

enum spp_tc_type {
	SPP_TC_UNSEGMENTED_CCSDS_EPOCH = 0x1,
	SPP_TC_UNSEGMENTED_CUSTOM_EPOCH = 0x2,
	SPP_TC_DAY_SEGMENTED = 0x4,
	SPP_TC_CALENDAR_SEGMENTED = 0x5,
	SPP_TC_CUSTOM = 0x6,
	SPP_TC_UNKNOWN = 0xff,
};

enum spp_tc_day_segmented_precision {
	SPP_TC_DAY_SEGMENTED_PRECISION_MS = 0x0,
	SPP_TC_DAY_SEGMENTED_PRECISION_US = 0x1,
	SPP_TC_DAY_SEGMENTED_PRECISION_PS = 0x2,
	SPP_TC_DAY_SEGMENTED_PRECISION_RESERVED = 0x3,
};

struct spp_tc_unsegmented_config_t {
	uint8_t base_unit_octets;
	uint8_t fractional_octets;
};

struct spp_tc_config_t {
	enum spp_tc_type type;
	union {
		struct spp_tc_unsegmented_config_t unsegmented;
		struct {
			bool use_custom_epoch;
			bool wide_day_segment;
			enum spp_tc_day_segmented_precision precision;
		} day_segmented;
	};
};

struct spp_tc_context_t {
	bool with_p_field;
	struct spp_tc_config_t defaults;
};

struct spp_tc_parser_preamble_state_t {
	bool has_second_octet;
	enum spp_tc_type detected_type;
};

struct spp_tc_parser_t {
	const struct spp_tc_context_t *ctx;
	enum spp_tc_parser_status status;
	struct spp_tc_config_t format;
	union {
		struct spp_tc_parser_preamble_state_t preamble;
		struct {
			bool read_base_unit;
			uint_least8_t base_unit_remaining;
			uint_least8_t fractional_remaining;
			uint64_t base_unit_count;
			uint64_t fractional_count;
			uint_least8_t fractional_ptr;
			uint8_t fractional_buf[
			    SPP_TC_UNSEGMENTED_FRACTIONAL_MAX_OCTETS];
		} unsegmented;
	} state;
};

struct spp_tc_context_t *spp_timecode_create_none(void);

int spp_tc_configure_from_preamble(struct spp_tc_context_t *ctx,
				   const uint8_t *preamble,
				   const size_t preamble_len);

void spp_tc_parser_init(const struct spp_tc_context_t *ctx,
			struct spp_tc_parser_t *parser);

int spp_tc_parser_feed(
		struct spp_tc_parser_t *parser,
		const uint8_t byte);

/**
 * @brief Get the number of octets used for the timestamp defined by the
 * context.
 *
 * @param ctx The context defining the timestamp.
 * @return Number of bytes for the timestamp or 0 if the timestamp cannot be
 * serialized.
 */
size_t spp_tc_get_size(const struct spp_tc_context_t *ctx);

/**
 * @brief Serialize a timestamp in the format as defined in the given ctx.
 *
 * A DTN timestamp in uD3TN consists of a 64 bit "seconds since Jan 1st, 2000"
 * value and a 32 bit counter.
 *
 * This is not compatible with any of the formats defined in the CCSDS.
 * Conversion happens as follows:
 *
 * * the actual time value of the DTN timestamp is converted as closely as
 *   possible to the timestamp as defined in ctx.
 * * the counter is used to generate a fake sub-second fractional time value
 *   based on the precision of the defined timestamp format.
 *
 * Since CCSDS timestamps only require monotonicity (but not strict
 * monotonicity) and DTN defines that the counter can only wrap at second
 * boundaries, this still yields legal (albeit slightly wrong) timestamps.
 *
 * @param ctx The timecode context defining the output format.
 * @param dtn_timestamp DTN timestamp time value.
 * @param dtn_counter DTN timestamp counter value.
 * @param out Pointer to a pointer into a buffer where the timestamp is
 * serialized. When the function returns, the pointer out points to the
 * next unused byte in the buffer.
 * @return Zero on success.
 */
int spp_tc_serialize(const struct spp_tc_context_t *ctx,
		     const uint64_t dtn_timestamp,
		     const uint32_t dtn_counter,
		     uint8_t **out);

/**
 * @brief Get the DTN timestamp parsed by a parser.
 *
 * This returns the timestamp as parsed. Retrieving the DTN timestamp may not
 * be supported with all parsed formats on all platforms.
 *
 * If the timestamp cannot be obtained, UINT64_MAX is returned. Possible reasons
 * for this condition:
 *
 * * parsing has not finished
 * * the conversion to DTN time is not supported for the parsed format
 * * the conversion to DTN time would yield a negative value
 *
 * @param parser The parser object to obtain the timestamp from.
 * @return The number of seconds since the DTN epoch (2000-01-01).
 */
uint64_t spp_tc_get_dtn_timestamp(const struct spp_tc_parser_t *parser);

#endif // SPP_TIMECODES_H
