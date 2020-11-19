#ifndef BUNDLE6_BUNDLE6_H_INCLUDED
#define BUNDLE6_BUNDLE6_H_INCLUDED

#include "ud3tn/bundle.h"

#include <stddef.h>
#include <stdint.h>

struct bundle6_eid_reference {
	uint16_t scheme_offset;
	uint16_t ssp_offset;
};

struct bundle6_eid_info {
	const char *str_ptr;
	const char *ssp_ptr;
	uint16_t dict_scheme_offset;
	uint16_t dict_ssp_offset;
};

struct bundle6_dict_descriptor {
	size_t dict_length_bytes;
	size_t eid_reference_count;
	struct bundle6_eid_info destination_eid_info;
	struct bundle6_eid_info source_eid_info;
	struct bundle6_eid_info report_to_eid_info;
	struct bundle6_eid_info custodian_eid_info;
	// can be written out by memcopying parts and appending zero terminators
	struct bundle6_eid_info eid_references[];
};

/**
 * Serializes the bundle dictionary into the given buffer
 */
void bundle6_serialize_dictionary(char *buf,
				  const struct bundle6_dict_descriptor *desc);

// ----------------------
// Bundle and block sizes
// ----------------------

size_t bundle6_get_dict_length(struct bundle *bundle);
struct bundle6_dict_descriptor *bundle6_calculate_dict(struct bundle *bundle);
void bundle6_recalculate_header_length(struct bundle *bundle);
size_t bundle6_get_serialized_size(struct bundle *bundle);


// -------------------
// Fragmentation sizes
// -------------------

size_t bundle6_get_first_fragment_min_size(struct bundle *bundle);
size_t bundle6_get_mid_fragment_min_size(struct bundle *bundle);
size_t bundle6_get_last_fragment_min_size(struct bundle *bundle);


#endif /* BUNDLE6_BUNDLE6_H_INCLUDED */
