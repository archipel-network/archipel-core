// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef MANAGEMENT_AGENT_H_
#define MANAGEMENT_AGENT_H_

#include "platform/hal_types.h"

#include <stdbool.h>

enum management_command {
	// Set the time of uD3TN, argument: DTN time (64 bit)
	MGMT_CMD_SET_TIME,
};

int management_agent_setup(QueueIdentifier_t bundle_processor_signaling_queue,
			   const char *local_eid,
			   bool allow_remote_configuration);

#endif // MANAGEMENT_AGENT_H_
