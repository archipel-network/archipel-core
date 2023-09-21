// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef ECHO_AGENT_H_
#define ECHO_AGENT_H_

#include "ud3tn/bundle_processor.h"

#include <stdint.h>

int echo_agent_setup(struct bundle_agent_interface *const bai,
		     const uint64_t lifetime_ms);

#endif // ECHO_AGENT_H_
