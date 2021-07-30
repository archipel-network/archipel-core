#include "agents/config_agent.h"
#include "agents/config_parser.h"

#include "ud3tn/agent_manager.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/common.h"

#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_types.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef REMOTE_CONFIGURATION
static const int ALLOW_REMOTE_CONFIGURATION = 1;
#else // REMOTE_CONFIGURATION
static const int ALLOW_REMOTE_CONFIGURATION;
#endif // REMOTE_CONFIGURATION

static struct config_parser parser;

static void router_command_send(struct router_command *cmd, void *param)
{
	struct router_signal signal = {
		.type = ROUTER_SIGNAL_PROCESS_COMMAND,
		.data = cmd
	};

	ASSERT(cmd != NULL);

	QueueIdentifier_t router_signaling_queue = param;

	hal_queue_push_to_back(router_signaling_queue, &signal);
}

static void callback(struct bundle_adu data, void *param)
{
	if (!ALLOW_REMOTE_CONFIGURATION) {
		if (strncmp((char *)param, data.source, strlen((char *)param)) != 0) {
			LOGF("ConfigAgent: Dropped config message from foreign endpoint",
			     data.source);
			return;
		}
	}

	config_parser_reset(&parser);
	config_parser_read(
		&parser,
		data.payload,
		data.length
	);
	bundle_adu_free_members(data);
}

int config_agent_setup(QueueIdentifier_t bundle_processor_signaling_queue,
	QueueIdentifier_t router_signaling_queue, const char *local_eid)
{
	ASSERT(config_parser_init(&parser, &router_command_send,
				  router_signaling_queue));
	return bundle_processor_perform_agent_action(
		bundle_processor_signaling_queue,
		BP_SIGNAL_AGENT_REGISTER,
		"config",
		callback,
		(void *)local_eid,
		false
	);
}
