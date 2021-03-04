#ifndef AGENT_MANAGER_H_INCLUDED
#define AGENT_MANAGER_H_INCLUDED

#include "ud3tn/bundle.h"

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

/**
 * @brief agent_forward Invoke the callback associated with the specified
 *	sink_identifier in the thread of the caller
 * @return int Return an error if the sink_identifier is unknown or the
 *	callback is NULL
 *
 * @not_thread_safe
 */
int agent_forward(const char *sink_identifier, struct bundle_adu data);


/**
 * @brief agent_register Register the callback and its param for the
 *	sink_identifier as an agent
 *
 * @param sink_identifier Unique string to identify an agent
 * @param callback Logic to be executed every time a bundle should be
 *	delivered to the agent
 * @param param Use this to pass additional arguments to callback
 * @return int Return an error if the sink_identifier is not unique or
 *	registration fails
 *
 * @not_thread_safe
 */
int agent_register(const char *sink_identifier,
		   void (*const callback)(struct bundle_adu data, void *param),
		   void *param);

/**
 * @brief agent_deregister Remove the agent associated with the specified
 *	sink_identifier
 * @return int Return an error if the sink_identifier is unknown or the
 *	deregistration fails
 *
 * @not_thread_safe
 */
int agent_deregister(const char *sink_identifier);

#endif /* AGENT_MANAGER_H_INCLUDED */
