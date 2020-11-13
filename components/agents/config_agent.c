#include "agents/config_agent.h"
#include "agents/config_parser.h"

#include "upcn/agent_manager.h"
#include "upcn/common.h"

#include "platform/hal_queue.h"
#include "platform/hal_types.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	(void)param;
	config_parser_reset(&parser);
	config_parser_read(
		&parser,
		data.payload,
		data.length
	);
	bundle_adu_free_members(data);
}

int config_agent_setup(QueueIdentifier_t router_signaling_queue)
{
	ASSERT(config_parser_init(&parser, &router_command_send,
				  router_signaling_queue));
	return agent_register("config", callback, NULL);
}
