#ifndef AGENT_MANAGER_H_INCLUDED
#define AGENT_MANAGER_H_INCLUDED

#include "upcn/bundle.h"

#include <stddef.h>
#include <stdint.h>

struct agent {
	const char *sink_identifier;
	void (*callback)(struct bundle_adu data, void *param);
	void *param;
};

struct agent_list {
	struct agent *agent_data;
	struct agent_list *next;
};

int agent_forward(const char *sink_identifier, struct bundle_adu data);

int agent_register(const char *sink_identifier,
		   void (*callback)(struct bundle_adu data, void *param),
		   void *param);

int agent_deregister(char *sink_identifier);

struct agent *agent_search(const char *sink_identifier);

#endif /* AGENT_MANAGER_H_INCLUDED */
