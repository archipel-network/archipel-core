#ifndef APPLICATIONAGENT_H_INCLUDED
#define APPLICATIONAGENT_H_INCLUDED

#include "ud3tn/bundle_agent_interface.h"

#include <stdint.h>

#define APPLICATION_AGENT_TASK_PRIORITY 2

#define APPLICATION_AGENT_BACKLOG 2

struct application_agent_config *application_agent_setup(
	const struct bundle_agent_interface *bundle_agent_interface,
	const char *socket_path,
	const char *node, const char *service,
	const uint8_t bp_version, uint64_t lifetime);

#endif /* APPLICATIONAGENT_H_INCLUDED */
