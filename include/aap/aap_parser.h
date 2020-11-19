#ifndef AAP_PARSER_H_INCLUDED
#define AAP_PARSER_H_INCLUDED

#include "aap/aap.h"

#include "ud3tn/parser.h"

#include <stddef.h>
#include <stdint.h>

struct aap_parser {
	/**
	 * Provides the current parser status, set by the `parse` method.
	 */
	enum parser_status status;

	/**
	 * Maximum allowed payload length. Can be set by the application.
	 */
	size_t max_payload_length;

	/**
	 * Parse the next part of the input. This function has to be called
	 * until the parser status provided in `status` changes to
	 * either PARSER_STATUS_DONE or PARSER_STATUS_ERROR.
	 */
	size_t (*parse)(struct aap_parser *,
			const uint8_t *buffer, size_t length);

	/**
	 * The parsed message. This field is only valid if the parser status
	 * is `PARSER_STATUS_DONE`. It should be accessed in a read-only manner.
	 * If the data should be used after a parser reset,
	 * `aap_parser_extract_message` has to be used to obtain the message.
	 */
	struct aap_message message;

	/*
	 * Internal fields follow.
	 */

	size_t consumed;
	size_t remaining;
};

/**
 * Initializes the AAP parser. This method shall only be called once.
 * For clearing the parser status, `aap_parser_reset` is provided.
 * However, for compatibility reasons this method might be called after
 * a call to `aap_parser_reset`.
 */
void aap_parser_init(struct aap_parser *parser);

/**
 * Resets the current parser state and frees allocated data.
 * If the parsed message should be used afterwards, it has to be obtained by
 * calling `aap_parser_extract_message` before calling this method.
 */
void aap_parser_reset(struct aap_parser *parser);

/**
 * Obtains the parsed message, possibly containing allocated fields.
 * The message is removed from the parser data structure.
 * The caller has to take care of freeing allocated fields using
 * `aap_message_clear`.
 */
struct aap_message aap_parser_extract_message(struct aap_parser *parser);

#endif // AAP_PARSER_H_INCLUDED
