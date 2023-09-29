// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef APPLICATIONAGENT_H_INCLUDED
#define APPLICATIONAGENT_H_INCLUDED

#include "ud3tn/bundle_processor.h"

#include <stdint.h>

#define APPLICATION_AGENT_TASK_PRIORITY 2

// Listen-backlog for the app. agent.
#ifndef APPLICATION_AGENT_BACKLOG
#define APPLICATION_AGENT_BACKLOG 2
#endif // APPLICATION_AGENT_BACKLOG

struct application_agent_config *application_agent_setup(
	const struct bundle_agent_interface *bundle_agent_interface,
	const char *socket_path,
	const char *node, const char *service,
	const uint8_t bp_version, uint64_t lifetime_ms);

#endif /* APPLICATIONAGENT_H_INCLUDED */
