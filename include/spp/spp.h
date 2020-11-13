#ifndef SPP_H_INCLUDED
#define SPP_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define SPP_MAX_APID (0x7ff)
#define SPP_MAX_SEGMENT_NUMBER (0x3fff)
#define SPP_MAX_DATA_LENGTH (65536)

enum spp_segment_status_t {
	SPP_SEGMENT_CONTINUATION = 0,
	SPP_SEGMENT_FIRST = 1,
	SPP_SEGMENT_LAST = 2,
	SPP_SEGMENT_UNSEGMENTED = 3
};

struct spp_meta_t {
	bool is_request;
	uint16_t apid;
	enum spp_segment_status_t segment_status;
	uint16_t segment_number;
	uint64_t dtn_timestamp;
	uint32_t dtn_counter;
};

struct spp_tc_context_t;
struct spp_context_t;

struct spp_context_t *spp_new_context(void);
void spp_free_context(struct spp_context_t *ctx);

bool spp_configure_ancillary_data(
		struct spp_context_t *ctx,
		size_t ancillary_data_length
		);

size_t spp_get_ancillary_data_length(const struct spp_context_t *ctx);

bool spp_configure_timecode(
		struct spp_context_t *ctx,
		struct spp_tc_context_t *timecode);


/**
 * @brief Get the size needed for an SPP with the given metadata.
 *
 * @param metadata The metadata to calculate the size for.
 * @return The size of a buffer which can contain the full packet.
 */
size_t spp_get_size(const struct spp_context_t *ctx,
					const size_t payload_len);

size_t spp_get_min_payload_size(const struct spp_context_t *ctx);
size_t spp_get_max_payload_size(const struct spp_context_t *ctx);

int spp_serialize_header(const struct spp_context_t *ctx,
			 const struct spp_meta_t *metadata,
			 const size_t payload_len,
			 uint8_t **out);

#endif
