#ifndef CLA_MTCP_PROTO_H
#define CLA_MTCP_PROTO_H

#include "ud3tn/parser.h"

#include <stddef.h>
#include <stdint.h>

void mtcp_parser_reset(struct parser *mtcp_parser);

size_t mtcp_parser_parse(struct parser *mtcp_parser,
			 const uint8_t *buffer,
			 size_t length);

size_t mtcp_encode_header(uint8_t *buffer, size_t buffer_size,
			  size_t data_length);

#endif // CLA_MTCP_PROTO_H
