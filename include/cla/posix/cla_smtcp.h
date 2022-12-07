// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef CLA_SMTCP_H
#define CLA_SMTCP_H

#include "cla/cla.h"

#include "ud3tn/bundle_processor.h"

#include <stddef.h>

struct cla_config *smtcp_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface);

#endif /* CLA_SMTCP_H */
