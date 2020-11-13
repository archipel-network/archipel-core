#include "spp/spp.h"
#include "spp/spp_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


static void write_uint16(const uint16_t val,
			 uint8_t *buf)
{
	buf[0] = val >> 8;
	buf[1] = val & 0xff;
}


static int spp_serialize_primary_header(
		const struct spp_primary_header_t *header,
		uint8_t *dest,
		uint8_t **next)
{
	if (header->apid > SPP_MAX_APID) {
		/* APID out of bounds */
		return -1;
	}
	if (header->data_length > SPP_MAX_DATA_LENGTH) {
		/* payload (including secondary header) too large */
		return -1;
	}
	if (header->segment_number > SPP_MAX_SEGMENT_NUMBER) {
		/* segment number out of bounds */
		return -1;
	}

	uint16_t part1 = 0;

	if (header->is_request) {
		/* set packet type bit */
		part1 |= SPP_PH_P1_TYPE_MASK;
	}
	if (header->has_secondary_header) {
		/* set bit indicating presence of secondary header */
		part1 |= SPP_PH_P1_SECONDARY_HEADER_FLAG_MASK;
	}
	part1 |= header->apid;

	const uint16_t part2 =
			(((uint16_t)header->segment_status)
			 << SPP_PH_P2_SEQUENCE_FLAGS_SHIFT)
			| header->segment_number;
	const uint16_t len = header->data_length - 1;

	write_uint16(part1, &dest[0]);
	write_uint16(part2, &dest[2]);
	write_uint16(len, &dest[4]);

	*next = &dest[6];

	return 0;
}


struct spp_context_t *spp_new_context(void)
{
	struct spp_context_t *ctx = malloc(sizeof(struct spp_context_t));

	ctx->ancillary_data_len = 0;
	ctx->timecode = NULL;
	return ctx;
}


int spp_serialize_header(const struct spp_context_t *ctx,
			 const struct spp_meta_t *metadata,
			 const size_t payload_len,
			 uint8_t **out)
{
	struct spp_primary_header_t header;

	header.apid = metadata->apid;
	header.data_length =
			payload_len + ctx->ancillary_data_len +
			(ctx->timecode ? spp_tc_get_size(ctx->timecode) : 0);
	header.has_secondary_header =
			ctx->ancillary_data_len > 0 || ctx->timecode != NULL;
	header.is_request = metadata->is_request;
	header.segment_status = metadata->segment_status;
	header.segment_number = metadata->segment_number;

	if (spp_serialize_primary_header(
				&header,
				&(*out)[0],
				out) != 0) {
		/* primary header serialization failed -> bail out */
		return -1;
	}

	if (ctx->timecode != NULL) {
		int status = spp_tc_serialize(ctx->timecode,
					      metadata->dtn_timestamp,
					      metadata->dtn_counter,
					      out);
		if (status != 0) {
			/* serialization of timestamp failed -> bail out */
			return -1;
		}
	}

	return 0;
}

size_t spp_get_size(const struct spp_context_t *ctx,
		    const size_t payload_len)
{
	if (payload_len == 0 && ctx->ancillary_data_len == 0 &&
			ctx->timecode == NULL) {
		/* invalid: len(payload) + len(ancillary) + len(timecode)
		 * must be > 0
		 */
		return 0;
	}
	return ctx->ancillary_data_len +
			payload_len +
			SPP_PRIMARY_HEADER_SIZE +
			(ctx->timecode ? spp_tc_get_size(ctx->timecode) : 0);
}

bool spp_configure_ancillary_data(struct spp_context_t *ctx,
				  size_t ancillary_data_length)
{
	if (ancillary_data_length > SPP_MAX_DATA_LENGTH) {
		/* ancillary data longer than maximum payload */
		return false;
	}

	// TODO: check with timecode length too!
	assert(ctx->timecode == NULL);

	ctx->ancillary_data_len = ancillary_data_length;
	return true;
}

size_t spp_get_ancillary_data_length(const struct spp_context_t *ctx)
{
	return ctx->ancillary_data_len;
}

bool spp_configure_timecode(struct spp_context_t *ctx,
			    struct spp_tc_context_t *timecode)
{
	ctx->timecode = timecode;
	return true;
}

void spp_free_context(struct spp_context_t *ctx)
{
	free(ctx);
}

size_t spp_get_min_payload_size(const struct spp_context_t *ctx)
{
	if (ctx->timecode || ctx->ancillary_data_len > 0) {
		/* if secondary header is present, there’s no minimum size for
		 * the actual payload
		 */
		return 0;
	}

	/* without secondary header, payload needs to be at least one
	 * byte long because the data length value in the primary header
	 * can’t be less than one byte.
	 */
	return 1;
}

size_t spp_get_max_payload_size(const struct spp_context_t *ctx)
{
	size_t result = SPP_MAX_DATA_LENGTH - ctx->ancillary_data_len;

	if (ctx->timecode != NULL) {
		/* subtract size of timecode secondary header part */
		result -= spp_tc_get_size(ctx->timecode);
	}

	return result;
}
