#ifndef BUNDLE_V7_PARSER_H_INCLUDED
#define BUNDLE_V7_PARSER_H_INCLUDED

#include "bundle7/eid.h"  // struct bundle7_eid_parser

#include "ud3tn/bundle.h"  // struct bundle
#include "ud3tn/crc.h"     // struct crc_stream
#include "ud3tn/parser.h"  // struct parser
#include "ud3tn/result.h"  // enum ud3tn_result

#include "cbor.h"

#include <limits.h>
#include <stddef.h>


#define BUNDLE7_DEFAULT_BUNDLE_QUOTA SIZE_MAX

enum bundle7_parser_flags {
	/**
	 * If the flag is set the parser will - after successfully parsing and
	 * CBOR element - feed the CRC calculation with the raw CBOR bytes of
	 * the parsed CBOR element.
	 */
	BUNDLE_V7_PARSER_CRC_FEED = 0x01,
};


/**
 * BPv7-bis CBOR parser using the iterator-bases CBOR-decoding from TinyCBOR
 */
struct bundle7_parser {
	struct parser *basedata;
	uint8_t flags;

	// CRC
	struct crc_stream crc16;
	struct crc_stream crc32;
	uint32_t checksum;

	/**
	 * Counts the number of CBOR bytes parsed for this bundle. This field
	 * is used to enforce the bundle quota (max. size of bundles)
	 */
	size_t bundle_size;
	struct bundle *bundle;

	/**
	 * Parsing callbacks
	 *
	 * Each callback can advance this function to pointer to transition
	 * into the next parsing stage.
	 */
	CborError (*parse)(struct bundle7_parser *, CborValue *);
	CborError (*next)(struct bundle7_parser *, CborValue *);

	/**
	 * The maximum block data size allowed for an incoming bundle.
	 * Can be updated at any time.
	 */
	size_t bundle_quota;

	/**
	 * Callback after a bundle gets successfully parsed. The only passed
	 * arguments are the bundle itself and an arbitrary parameter passed
	 * upon parser initialization.
	 */
	void (*send_callback)(struct bundle *, void *);
	void *send_param;

	struct bundle_block_list **current_block_entry;
};


struct parser *bundle7_parser_init(
	struct bundle7_parser *state,
	void (*send_callback)(struct bundle *, void *),
	void *param);


size_t bundle7_parser_read(
	struct bundle7_parser *parser,
	const uint8_t *buffer,
	size_t length);

enum ud3tn_result bundle7_parser_reset(struct bundle7_parser *state);
enum ud3tn_result bundle7_parser_deinit(struct bundle7_parser *state);


#endif /* BUNDLE_V7_PARSER_H_INCLUDED */
