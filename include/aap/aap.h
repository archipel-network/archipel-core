#ifndef AAP_H_INCLUDED
#define AAP_H_INCLUDED

#include "ud3tn/bundle.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

enum aap_message_type {
	/**
	 * Positive acknowledgment
	 */
	AAP_MESSAGE_ACK = 0x0,
	/**
	 * Negative acknowledgment
	 */
	AAP_MESSAGE_NACK = 0x1,
	/**
	 * EID registration request (from application)
	 */
	AAP_MESSAGE_REGISTER = 0x2,
	/**
	 * Bundle transmission request (from application)
	 */
	AAP_MESSAGE_SENDBUNDLE = 0x3,
	/**
	 * Bundle reception message (to application)
	 */
	AAP_MESSAGE_RECVBUNDLE = 0x4,
	/**
	 * Bundle transmission confirmation (to application)
	 */
	AAP_MESSAGE_SENDCONFIRM = 0x5,
	/**
	 * Bundle cancellation request (from application)
	 */
	AAP_MESSAGE_CANCELBUNDLE = 0x6,
	/**
	 * Connection establishment notice (to application)
	 */
	AAP_MESSAGE_WELCOME = 0x7,
	/**
	 * Connection liveliness check
	 * (both direction, also for keepalive purposes)
	 */
	AAP_MESSAGE_PING = 0x8,
	/**
	 * Not part of the on-wire format:
	 * This code explicitly marks an in-memory AAP message invalid.
	 */
	AAP_MESSAGE_INVALID = 0xFF,
};

struct aap_message {
	/**
	 * The message type.
	 */
	enum aap_message_type type;

	/**
	 * An endpoint identifier according to the message type,
	 * valid in the REGISTER, SENDBUNDLE, RECVBUNDLE, and WELCOME messages.
	 *
	 * - In the REGISTER message, this contains the sub-EID
	 *   requested for registration in uD3TN.
	 * - In the SENDBUNDLE message, this contains the destination EID.
	 * - In the RECVBUNDLE message, this contains the source EID.
	 * - In the WELCOME message, this contains the base EID of uD3TN.
	 */
	char *eid;
	/**
	 * The length of the contained EID string, without any terminating
	 * zero-character.
	 */
	size_t eid_length;

	/**
	 * The bundle payload data,
	 * valid in the SENDBUNDLE and RECVBUNDLE messages.
	 * This field can be NULL if `payload_length` is 0.
	 */
	uint8_t *payload;
	/**
	 * The size of the bundle payload message.
	 */
	size_t payload_length;

	/**
	 * The identifier of the bundle,
	 * valid in the SENDCONFIRM and CANCELBUNDLE messages.
	 */
	bundleid_t bundle_id;
};

/**
 * Returns a boolean value indicating whether the provided AAP message is valid.
 */
bool aap_message_is_valid(const struct aap_message *message);

/**
 * Resets the message to an invalid state and frees all allocated fields.
 */
void aap_message_clear(struct aap_message *const message);

#endif // AAP_H_INCLUDED
