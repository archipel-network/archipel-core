// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef AAP2_AGENT_H_INCLUDED
#define AAP2_AGENT_H_INCLUDED

#include "ud3tn/bundle_processor.h"

#include <stdint.h>

#define AAP2_AGENT_TASK_PRIORITY 2

#define AAP2_AGENT_BACKLOG 2

struct aap2_agent_config *aap2_agent_setup(
	const struct bundle_agent_interface *bundle_agent_interface,
	const char *socket_path,
	const char *node, const char *service,
	const uint8_t bp_version, uint64_t lifetime_ms);

#endif /* AAP2_AGENT_H_INCLUDED */
