#ifndef CLA_BIBE_PROTO_H
#define CLA_BIBE_PROTO_H

#include "ud3tn/parser.h"
#include "bundle7/reports.h"

#include <stddef.h>
#include <stdint.h>

void bibe_parser_reset(struct parser *bibe_parser);

size_t bibe_parser_parse(
			 const uint8_t *buffer,
			 size_t length,
			 struct bibe_protocol_data_unit *bpdu
			 );

#endif // CLA_BIBE_PROTO_H
