// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef CLA_BIBE_PROTO_H
#define CLA_BIBE_PROTO_H

#include "bundle7/reports.h"

#include "ud3tn/parser.h"

#include <stddef.h>
#include <stdint.h>

struct bibe_header {
	size_t hdr_len;
	uint8_t *data;
};

size_t bibe_parser_parse(const uint8_t *buffer, size_t length,
			 struct bibe_protocol_data_unit *bpdu);

struct bibe_header bibe_encode_header(const char *dest_eid, size_t payload_len);

#endif // CLA_BIBE_PROTO_H
