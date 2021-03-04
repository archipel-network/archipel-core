#ifndef CONFIGAGENT_H_INCLUDED
#define CONFIGAGENT_H_INCLUDED

#include "platform/hal_types.h"

#include <stddef.h>
#include <stdint.h>

#define CONFIG_AGENT_QUEUE_SIZE 10
#define CONFIG_AGENT_TASK_PRIORITY 2

#ifdef CONFIG_AGENT_REMOTE_CONFIGURATION
static const int ALLOW_REMOTE_CONFIGURATION = 1;
#else // CONFIG_AGENT_REMOTE_CONFIGURATION
static const int ALLOW_REMOTE_CONFIGURATION;
#endif // CONFIG_AGENT_REMOTE_CONFIGURATION

struct config_agent_item {
	uint8_t *data;
	size_t data_length;
};

int config_agent_setup(QueueIdentifier_t bundle_processor_signaling_queue,
	QueueIdentifier_t router_signaling_queue, const char *local_eid);

#endif /* CONFIGAGENT_H_INCLUDED */
