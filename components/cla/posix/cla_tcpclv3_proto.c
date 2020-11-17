#include "cla/posix/cla_tcpclv3_proto.h"

#include "bundle6/sdnv.h"

#include "platform/hal_io.h"

#include "ud3tn/common.h"
#include "ud3tn/parser.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// HANDSHAKING

char *cla_tcpclv3_generate_contact_header(
	const char *const local_eid, size_t *len)
{
	const size_t local_eid_len = strlen(local_eid);

	char sdnv_buffer[MAX_SDNV_SIZE];
	const size_t sdnv_len =
		sdnv_write_u32((uint8_t *)sdnv_buffer, (uint32_t)local_eid_len);

	// Allocate memory for header packet.
	const size_t header_len = 8 + sdnv_len + local_eid_len;
	char *const header_packet = malloc(header_len);

	if (!header_packet)
		return NULL;

	// Write magic into packet (string "dtn!").
	header_packet[0] = 'd';
	header_packet[1] = 't';
	header_packet[2] = 'n';
	header_packet[3] = '!';

	// Put version number into packet (RFC7242 -> 3).
	header_packet[4] = 0x03;

	// Set flags (currently we don't support any additional tcpcl features)
	// 0x00 == NO ACK, NO FRAG, NO REFUSAL, NO LENGTH MESSAGES
	header_packet[5] = 0x00;

	// Set keep_alive interval (currently not supported -> 0 == disable).
	header_packet[6] = 0x00;
	header_packet[7] = 0x00;

	// Add sdnv value.
	memcpy(&header_packet[8], sdnv_buffer, sdnv_len);

	// Add eid value.
	memcpy(&header_packet[8 + sdnv_len], local_eid, local_eid_len);

	*len = header_len;
	return header_packet;
}




// PARSER

void tcpclv3_parser_read_byte(struct tcpclv3_parser *parser, const uint8_t byte)
{
	switch (parser->stage) {
	case TCPCLV3_EXPECT_TYPE_FLAGS:
		if ((byte & 0xF0) == TCPCLV3_TYPE_DATA_SEGMENT &&
				(byte & TCPCLV3_FLAG_S) != 0 &&
				(byte & TCPCLV3_FLAG_E) != 0) {
			parser->type = TCPCLV3_TYPE_DATA_SEGMENT;
			parser->stage = TCPCLV3_GET_SIZE;
		} else {
			parser->basedata.status = PARSER_STATUS_ERROR;
			tcpclv3_parser_reset(parser);
		}
		/*
		 * Something went wrong and we have to reset the parser
		 * and wait for another proper tcpcl packet
		 */
		break;

	case TCPCLV3_GET_SIZE:
		sdnv_read_u32(
			&parser->sdnv_state,
			&parser->fragment_size,
			byte
		);

		switch (parser->sdnv_state.status) {
		case SDNV_IN_PROGRESS:
			/* intentional, do nothing unless
			 * a termination state is reached
			 */
			break;
		case SDNV_DONE:
			if (parser->type == TCPCLV3_TYPE_DATA_SEGMENT) {
				parser->stage = TCPCLV3_FORWARD_BUNDLE;
			} else {
				LOG("tcpclv3_parser: wrong type detected");
				parser->basedata.status = PARSER_STATUS_ERROR;
				tcpclv3_parser_reset(parser);
			}
			sdnv_reset(&parser->sdnv_state);
			break;
		case SDNV_ERROR:
			LOG("tcpclv3_parser: SDNV error");
			sdnv_reset(&parser->sdnv_state);
			parser->basedata.status = PARSER_STATUS_ERROR;
			break;
		}
		break;

	default:
		parser->basedata.status = PARSER_STATUS_ERROR;
		break;
	}
}

void tcpclv3_parser_reset(struct tcpclv3_parser *parser)
{
	parser->basedata.status = PARSER_STATUS_GOOD;
	parser->basedata.flags = PARSER_FLAG_NONE;
	parser->stage = TCPCLV3_EXPECT_TYPE_FLAGS;
	parser->type = TCPCLV3_TYPE_UNDEFINED;
	free(parser->string);
	parser->string = NULL;
	parser->intdata = 0;
	parser->intdata_index = 0;
}

struct parser *tcpclv3_parser_init(struct tcpclv3_parser *parser)
{
	parser->string = NULL;
	tcpclv3_parser_reset(parser);
	sdnv_reset(&parser->sdnv_state);
	return &parser->basedata;
}

size_t tcpclv3_parser_read(struct tcpclv3_parser *parser,
	const uint8_t *buffer, size_t length)
{
	size_t i = 0;

	while (i < length &&
			parser->basedata.status == PARSER_STATUS_GOOD &&
			parser->stage != TCPCLV3_FORWARD_BUNDLE) {
		tcpclv3_parser_read_byte(parser, buffer[i]);
		i++;
	}

	return i;
}
