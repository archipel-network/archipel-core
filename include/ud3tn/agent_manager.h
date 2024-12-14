// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef AGENT_MANAGER_H_INCLUDED
#define AGENT_MANAGER_H_INCLUDED

#include "ud3tn/bundle.h"
#include "platform/hal_types.h"

#include <stddef.h>
#include <stdint.h>

struct agent {
	const char *sink_identifier;
	const char *secret;
	void (*callback)(struct bundle_adu data, void *param,
			 const void *bp_context);
	void *param;
};

struct agent_list {
	struct agent agent_data;
	struct agent_list *next;
};

/**
 * @brief Initialize the agent manager for the given local EID.
 *
 * @not_thread_safe
 */
void agent_manager_init(const char *const ud3tn_local_eid, QueueIdentifier_t restore_queue);

/**
 * @brief agent_forward Invoke the callback associated with the specified
 *	sink_identifier in the thread of the caller
 * @return int Return an error if the sink_identifier is unknown or the
 *	callback is NULL
 *
 * @not_thread_safe
 */
int agent_forward(const char *sink_identifier, struct bundle_adu data,
		  const void *bp_context);


/**
 * @brief agent_register Register the specified agent
 *
 * @param agent The agent to be registered
 * @param is_subscriber Whether to receive bundles with this agent or not
 * @return int Return an error if the sink_identifier is not unique or
 *	registration fails
 *
 * @not_thread_safe
 */
int agent_register(struct agent agent, bool is_subscriber);

/**
 * @brief agent_deregister Remove the agent associated with the specified
 *	sink_identifier
 * @param sink_identifier The identifier of the agent to be unregistered
 * @param is_subscriber Whether to receive bundles with this agent or not
 * @return int Return an error if the sink_identifier is unknown or the
 *	deregistration fails
 *
 * @not_thread_safe
 */
int agent_deregister(const char *sink_identifier, bool is_subscriber);

/**
 * @brief Check if named agent is currently registered
 * @param agent_id identifier of agent to check presence
 * @return true if agent is registered, false if not
 */
bool is_agent_available(const char *agent_id);

#endif /* AGENT_MANAGER_H_INCLUDED */
