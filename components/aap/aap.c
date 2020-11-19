#include "aap/aap.h"

#include "ud3tn/bundle.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// Ignore comparison warning if underlying int for
// enum aap_message_type is unsigned.
#pragma GCC diagnostic ignored "-Wtype-limits"

bool aap_message_is_valid(const struct aap_message *const msg)
{
	if (msg->type < AAP_MESSAGE_ACK ||
	    msg->type > AAP_MESSAGE_PING)
		return false;

	// EID
	if (msg->type == AAP_MESSAGE_REGISTER ||
	    msg->type == AAP_MESSAGE_SENDBUNDLE ||
	    msg->type == AAP_MESSAGE_RECVBUNDLE ||
	    msg->type == AAP_MESSAGE_WELCOME) {
		if (msg->eid == NULL || msg->eid_length > UINT16_MAX ||
		    strlen(msg->eid) != msg->eid_length)
			return false;
	} else {
		if (msg->eid != NULL)
			return false;
	}

	// Payload
	if (msg->type == AAP_MESSAGE_SENDBUNDLE ||
	    msg->type == AAP_MESSAGE_RECVBUNDLE) {
		if (msg->payload == NULL && msg->payload_length != 0)
			return false;
	} else {
		if (msg->payload != NULL)
			return false;
	}

	// Bundle ID
	if (msg->type == AAP_MESSAGE_SENDCONFIRM ||
	    msg->type == AAP_MESSAGE_CANCELBUNDLE) {
		if (msg->bundle_id == BUNDLE_INVALID_ID)
			return false;
	}

	return true;
}

void aap_message_clear(struct aap_message *const message)
{
	if (message->eid)
		free(message->eid);
	if (message->payload)
		free(message->payload);
	memset(message, 0, sizeof(struct aap_message));
	message->type = AAP_MESSAGE_INVALID;
}
