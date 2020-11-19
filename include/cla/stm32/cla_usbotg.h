#ifndef CLA_USBOTG
#define CLA_USBOTG

#include "cla/cla.h"

#include "ud3tn/bundle_agent_interface.h"

#include <stddef.h>

struct cla_config *usbotg_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface);

#endif // CLA_USBOTG
