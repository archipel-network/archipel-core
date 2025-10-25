// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef ARCHIPEL_CORE
#ifndef CLA_KISS_UNI_H_INCLUDED
#define CLA_KISS_UNI_H_INCLUDED

#include <stddef.h>
#include "ud3tn/bundle_processor.h"

/**
* Create KISS Unidirectionnal CLA
*/
struct cla_config *kissunicla_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface);

#endif
#endif