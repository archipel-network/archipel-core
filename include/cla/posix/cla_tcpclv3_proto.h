#ifndef CLA_TCPCLV3PROTO_H_INCLUDED
#define CLA_TCPCLV3PROTO_H_INCLUDED

#include "bundle6/sdnv.h"

#include "ud3tn/parser.h"

#include <stddef.h>
#include <stdint.h>

enum tcpclv3_type {
	TCPCLV3_TYPE_UNDEFINED    = 0x00,
	TCPCLV3_TYPE_DATA_SEGMENT = 0x10,
};

enum tcpclv3_flags {
	TCPCLV3_FLAG_E = 0x01,
	TCPCLV3_FLAG_S = 0x02,
};

enum tcpclv3_parser_stage {
	TCPCLV3_EXPECT_TYPE_FLAGS,
	TCPCLV3_GET_SIZE,
	TCPCLV3_FORWARD_BUNDLE
};

struct tcpclv3_parser {
	struct parser basedata;
	enum tcpclv3_parser_stage stage;
	enum tcpclv3_type type;
	char *string;
	struct sdnv_state sdnv_state;
	uint32_t fragment_size;
	uint32_t intdata;
	uint16_t intdata_index;
};

// HANDSHAKE

char *cla_tcpclv3_generate_contact_header(
	const char *const local_eid, size_t *len);

// SERIALIZER

// PARSER

struct parser *tcpclv3_parser_init(struct tcpclv3_parser *parser);
void tcpclv3_parser_read_byte(struct tcpclv3_parser *parser, uint8_t byte);
void tcpclv3_parser_reset(struct tcpclv3_parser *parser);
size_t tcpclv3_parser_read(struct tcpclv3_parser *parser,
			   const uint8_t *buffer, size_t length);

#endif // CLA_TCPCLV3PROTO_H_INCLUDED
