#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

#include <stddef.h>

enum parser_status {
	PARSER_STATUS_GOOD,
	PARSER_STATUS_DONE,
	PARSER_STATUS_ERROR,
};

enum parser_flags {
	PARSER_FLAG_NONE = 0x00,
	PARSER_FLAG_BULK_READ = 0x01,

	/**
	 * Performing a "chunked" input read where the parser reads
	 * blocks of bytes from them the "next_buffer"
	 */
	PARSER_FLAG_CHUNKED_READ = 0x02,

	/**
	 * If the parser status is set to "PARSER_STATUS_ERROR" this flag
	 * indicates that the parser failed due to an invalid CRC.
	 */
	PARSER_FLAG_CRC_INVALID = 0x04,

	/**
	 * The parser is forwarding data to a subparser.
	 */
	PARSER_FLAG_DATA_SUBPARSER = 0x08,
};

struct parser {
	enum parser_status status;
	enum parser_flags flags;
	void *next_buffer;

	/**
	 * If a parser is operating in "bulk read" mode, this field states
	 * how many bytes have to be read into the "next_buffer" until the
	 * input processor forwards the data to the parser.
	 * If a parser is performing subparser forwarding, this field states
	 * the amount of bytes to be forwarded.
	 */
	size_t next_bytes;
};

#endif /* PARSER_H_INCLUDED */
