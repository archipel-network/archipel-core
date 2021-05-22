#ifndef BLACKHOLE_PARSER_H_INCLUDED
#define BLACKHOLE_PARSER_H_INCLUDED

#include "ud3tn/parser.h"
#include "ud3tn/result.h"

#include <stddef.h>
#include <stdint.h>

struct blackhole_parser {
	struct parser *basedata;
	uint64_t to_read;
};

struct parser *blackhole_parser_init(struct blackhole_parser *parser);
enum ud3tn_result blackhole_parser_reset(struct blackhole_parser *parser);
enum ud3tn_result blackhole_parser_deinit(struct blackhole_parser *parser);

size_t blackhole_parser_read(struct blackhole_parser *parser,
	const uint8_t *buffer, size_t length);

#endif /* BLACKHOLE_PARSER_H_INCLUDED */
