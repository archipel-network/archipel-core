// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef CLA_TCPSPP_CONFIG_H
#define CLA_TCPSPP_CONFIG_H

#include "cla/cla.h"

#include "ud3tn/bundle_processor.h"

#include <stddef.h>

#ifndef CLA_TCPSPP_TIMESTAMP_FORMAT_PREAMBLE
#define CLA_TCPSPP_TIMESTAMP_FORMAT_PREAMBLE 0x1c
#endif

#ifndef CLA_TCPSPP_TIMESTAMP_USE_P_FIELD
#define CLA_TCPSPP_TIMESTAMP_USE_P_FIELD (true)
#endif

#ifndef CLA_TCPSPP_USE_CRC
#define CLA_TCPSPP_USE_CRC (true)
#endif

#ifndef CLA_TCPSPP_SPP_MAX_SIZE
#define CLA_TCPSPP_SPP_MAX_SIZE (1 << 16)
#endif

struct cla_config *tcpspp_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface);

#endif /* CLA_TCPSPP_CONFIG_H */
