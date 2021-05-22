#include "cla/blackhole_parser.h"

#include "ud3tn/parser.h"
#include "ud3tn/result.h"

#include <stddef.h>
#include <stdlib.h>


struct parser *blackhole_parser_init(struct blackhole_parser *parser)
{
	parser->basedata = malloc(sizeof(struct parser));
	if (parser->basedata == NULL)
		return NULL;
	if (blackhole_parser_reset(parser) != UD3TN_OK)
		return NULL;
	return parser->basedata;
}

enum ud3tn_result blackhole_parser_reset(struct blackhole_parser *parser)
{
	parser->basedata->status = PARSER_STATUS_GOOD;
	parser->basedata->flags = PARSER_FLAG_NONE;
	parser->to_read = 0;
	return UD3TN_OK;
}

enum ud3tn_result blackhole_parser_deinit(struct blackhole_parser *parser)
{
	free(parser->basedata);
	return UD3TN_OK;
}

// NOTE: This function will never enter nor be called from BULK_READ mode.
size_t blackhole_parser_read(struct blackhole_parser *parser,
	const uint8_t *buffer, size_t length)
{
	size_t read_bytes;

	(void)buffer;
	if (length >= parser->to_read)
		read_bytes = parser->to_read;
	else
		read_bytes = length;

	parser->to_read -= read_bytes;

	if (!parser->to_read)
		parser->basedata->status = PARSER_STATUS_DONE;

	return read_bytes;
}
