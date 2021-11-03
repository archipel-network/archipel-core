#include "agents/config_agent.h"
#include "agents/config_parser.h"

#include "ud3tn/bundle_processor.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"

#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct config_parser parser;

struct config_agent_params {
	const char *local_eid;
	bool allow_remote_configuration;
};

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
	struct config_agent_params *const ca_param = param;

	if (!ca_param->allow_remote_configuration) {
		if (strncmp(ca_param->local_eid, data.source,
		    strlen(ca_param->local_eid)) != 0) {
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
	QueueIdentifier_t router_signaling_queue, const char *local_eid,
	bool allow_remote_configuration)
{
	int is_ipn = (memcmp(local_eid, "ipn", 3) == 0);

	ASSERT(config_parser_init(&parser, &router_command_send,
				  router_signaling_queue));

	struct config_agent_params *const ca_param = malloc(
		sizeof(struct config_agent_params)
	);
	ca_param->local_eid = local_eid;
	ca_param->allow_remote_configuration = allow_remote_configuration;

	return bundle_processor_perform_agent_action(
		bundle_processor_signaling_queue,
		BP_SIGNAL_AGENT_REGISTER,
		is_ipn ? AGENT_ID_CONFIG_IPN : AGENT_ID_CONFIG_DTN,
		callback,
		ca_param,
		false
	);
}
