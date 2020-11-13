#ifndef SPP_INTERNAL_H
#define SPP_INTERNAL_H

#include "spp/spp.h"
#include "spp/spp_timecodes.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define SPP_PRIMARY_HEADER_SIZE (6)
#define SPP_PH_P1_VERSION_MASK (0xe000)
#define SPP_PH_EARLY_VERSION_MASK (SPP_PH_P1_VERSION_MASK >> 8)
#define SPP_PH_P1_TYPE_MASK (0x1000)
#define SPP_PH_P1_SECONDARY_HEADER_FLAG_MASK (0x0800)
#define SPP_PH_P1_APID_MASK (0x07ff)
#define SPP_PH_P2_SEQUENCE_FLAGS_MASK (0xc000)
#define SPP_PH_P2_SEQUENCE_FLAGS_SHIFT (14)
#define SPP_PH_P2_SEQUENCE_CNT_MASK (0x3fff)


struct spp_primary_header_t {
	bool is_request;
	bool has_secondary_header;
	uint16_t apid;
	enum spp_segment_status_t segment_status;
	uint16_t segment_number;
	size_t data_length;
};


struct spp_context_t {
	struct spp_tc_context_t *timecode;
	size_t ancillary_data_len;
};


bool spp_check_first_byte(uint8_t byte);
void spp_parse_ph_p1(const uint16_t word,
					 struct spp_primary_header_t *header);
void spp_parse_ph_p2(const uint16_t word,
					 struct spp_primary_header_t *header);
void spp_parse_ph_len(const uint16_t word,
					  struct spp_primary_header_t *header);
int spp_parse_primary_header(
		const uint8_t *buf,
		const size_t length,
		struct spp_primary_header_t *header,
		const uint8_t **next);

#endif // SPP_INTERNAL_H
