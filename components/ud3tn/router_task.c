// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

#include "ud3tn/bundle.h"
#include "ud3tn/bundle_fragmenter.h"
#include "ud3tn/common.h"
#include "ud3tn/node.h"
#include "ud3tn/router.h"
#include "ud3tn/routing_table.h"

#include "platform/hal_io.h"
#include "platform/hal_time.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// COMMAND HANDLING

static bool process_router_command(
	struct router_command *router_cmd,
	struct rescheduling_handle rescheduler)
{
	switch (router_cmd->type) {
	case ROUTER_COMMAND_ADD:
		return routing_table_add_node(
			router_cmd->data,
			rescheduler
		);
	case ROUTER_COMMAND_UPDATE:
		return routing_table_replace_node(
			router_cmd->data,
			rescheduler
		);
	case ROUTER_COMMAND_DELETE:
		return routing_table_delete_node(
			router_cmd->data,
			rescheduler
		);
	default:
		free_node(router_cmd->data);
		return false;
	}
}

enum ud3tn_result router_process_command(
	struct router_command *command,
	struct rescheduling_handle rescheduler)
{
	bool success = true;
	const uint64_t cur_time_s = hal_time_get_timestamp_s();

	if (!node_prepare_and_verify(command->data, cur_time_s)) {
		free_node(command->data);
		LOGF_WARN("Router: Command (T = %c) is invalid!",
			command->type);
		free(command);
		return UD3TN_FAIL;
	}

	success = process_router_command(
		command,
		rescheduler
	);
	if (success) {
		LOGF_DEBUG(
			"Router: Command (T = %c) processed.",
			command->type
		);
	} else {
		LOGF_DEBUG(
			"Router: Processing command (T = %c) failed!",
			command->type
		);
	}
	free(command);

	return success ? UD3TN_OK : UD3TN_FAIL;
}