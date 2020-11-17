#ifndef BUNDLE_AGENT_INTERFACE_H
#define BUNDLE_AGENT_INTERFACE_H

#include "platform/hal_types.h"

// Interface to the bundle agent, provided to other agents and the CLA.
struct bundle_agent_interface {
	char *local_eid;

	QueueIdentifier_t bundle_signaling_queue;
	QueueIdentifier_t router_signaling_queue;
};

#endif // BUNDLE_AGENT_INTERFACE_H
