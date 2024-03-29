// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef CONFIGAGENT_H_INCLUDED
#define CONFIGAGENT_H_INCLUDED

#include "platform/hal_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONFIG_AGENT_QUEUE_SIZE 10
#define CONFIG_AGENT_TASK_PRIORITY 2

// Default Agent IDs.
#ifndef AGENT_ID_CONFIG_DTN
#define AGENT_ID_CONFIG_DTN "config"
#endif // AGENT_ID_CONFIG_DTN
#ifndef AGENT_ID_CONFIG_IPN
#define AGENT_ID_CONFIG_IPN "9000"
#endif // AGENT_ID_CONFIG_IPN

struct config_agent_item {
	uint8_t *data;
	size_t data_length;
};

int config_agent_setup(
	QueueIdentifier_t bundle_processor_signaling_queue,
	const char *local_eid,
	bool allow_remote_configuration,
	void *bundle_processor_context);

#endif /* CONFIGAGENT_H_INCLUDED */
