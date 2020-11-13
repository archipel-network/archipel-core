#include "spp/spp_internal.h"


static uint16_t read_uint16(const uint8_t *buf)
{
	return (buf[0] << 8) | buf[1];
}


bool spp_check_first_byte(uint8_t byte)
{
	if ((byte & SPP_PH_EARLY_VERSION_MASK) != 0) {
		/* version bits must be 0b000 ( = version 1) */
		return false;
	}
	return true;
}


void spp_parse_ph_p1(const uint16_t word,
		     struct spp_primary_header_t *header)
{
	header->has_secondary_header =
			(word & SPP_PH_P1_SECONDARY_HEADER_FLAG_MASK) != 0;
	header->is_request = (word & SPP_PH_P1_TYPE_MASK) != 0;
	header->apid = word & SPP_PH_P1_APID_MASK;
}


void spp_parse_ph_p2(const uint16_t word,
		     struct spp_primary_header_t *header)
{
	header->segment_status =
			(word
			 & SPP_PH_P2_SEQUENCE_FLAGS_MASK)
			>> SPP_PH_P2_SEQUENCE_FLAGS_SHIFT;
	header->segment_number = (word & SPP_PH_P2_SEQUENCE_CNT_MASK);
}


void spp_parse_ph_len(const uint16_t word,
		      struct spp_primary_header_t *header)
{
	// next length is always at least one
	header->data_length = word + 1;
}


int spp_parse_primary_header(
		const uint8_t *buf,
		const size_t length,
		struct spp_primary_header_t *header,
		const uint8_t **next)
{
	if (length < SPP_PRIMARY_HEADER_SIZE) {
		/* buffer too short for a primary header */
		return -1;
	}

	if (!spp_check_first_byte(buf[0])) {
		/* version mismatch */
		return -1;
	}

	if (next) {
		/* set the next pointer to the first byte after the header */
		*next = &buf[6];
	}

	spp_parse_ph_p1(read_uint16(&buf[0]), header);
	spp_parse_ph_p2(read_uint16(&buf[2]), header);
	spp_parse_ph_len(read_uint16(&buf[4]), header);

	if ((length - SPP_PRIMARY_HEADER_SIZE) < header->data_length) {
		/* advertised data length is greater than the full buffer */
		// TODO: do we want the check here?
		return -1;
	}

	return 0;
}
