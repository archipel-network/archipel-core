#include "agents/application_agent.h"

#include "platform/hal_types.h"

#include <stdint.h>
#include <stdlib.h>

struct application_agent_config *application_agent_setup(
	const struct bundle_agent_interface *bundle_agent_interface,
	const char *socket_path,
	const char *node, const char *service,
	const uint8_t bp_version, uint64_t lifetime)
{
	(void)bundle_agent_interface;
	(void)socket_path;
	(void)node;
	(void)service;
	(void)bp_version;
	(void)lifetime;

	// STUB for STM32 - application agent is not supported there.
	return malloc(1);
}
