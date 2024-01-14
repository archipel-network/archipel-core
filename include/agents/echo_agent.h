// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef ECHO_AGENT_H_
#define ECHO_AGENT_H_

#include "ud3tn/bundle_processor.h"

#include <stdint.h>

// Default Agent IDs.
#ifndef AGENT_ID_ECHO_DTN
#define AGENT_ID_ECHO_DTN "echo"
#endif // AGENT_ID_ECHO_DTN
#ifndef AGENT_ID_ECHO_IPN
#define AGENT_ID_ECHO_IPN "9002"
#endif // AGENT_ID_ECHO_IPN

int echo_agent_setup(struct bundle_agent_interface *const bai,
		     const uint64_t lifetime_ms);

#endif // ECHO_AGENT_H_
