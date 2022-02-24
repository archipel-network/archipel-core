// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef AAP_SERIALIZER_H_INCLUDED
#define AAP_SERIALIZER_H_INCLUDED

#include "aap/aap.h"

#include <stddef.h>

/**
 * Returns the serialized size of the specified message.
 * The message has to be valid according to `aap_message_is_valid`.
 */
size_t aap_get_serialized_size(const struct aap_message *msg);

/**
 * Serializes the specified message by repeatedly calling the provided function.
 * The message has to be valid according to `aap_message_is_valid`.
 *
 * @param msg The message to be serialized.
 * @param write A function to write out / send the serialized message.
 * @param param An arbitrary parameter passed to the write function as
 *              first argument.
 * @param serialize_pl Flag to enable / disable serialization of the
 *				message payload.
 */
void aap_serialize(const struct aap_message *msg,
	void (*write)(void *param, const void *data, const size_t length),
	void *param, const bool serialize_pl);

/**
 * Serializes the specified message into the provided buffer.
 * The buffer size has to be equal or greater than what is returned by
 * `aap_get_serialized_size`. Exactly this amount of bytes will be written.
 * The message has to be valid according to `aap_message_is_valid`.
 *
 * @param buffer The destination buffer.
 * @param msg The message to be serialized.
 * @param serialize_pl Flag to enable / disable serialization of the
 *				message payload.
 */
void aap_serialize_into(void *buffer, const struct aap_message *msg,
			const bool serialize_pl);

#endif // AAP_SERIALIZER_H_INCLUDED
