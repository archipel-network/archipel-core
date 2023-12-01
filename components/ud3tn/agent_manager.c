// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/agent_manager.h"
#include "ud3tn/bundle.h"
#include "ud3tn/eid.h"

#include "agents/config_agent.h"
#include "agents/application_agent.h"

#include "platform/hal_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static struct agent *agent_search(struct agent_list **al_ptr,
				  const char *sink_identifier);
static int agent_list_add_entry(struct agent_list **al_ptr,
				struct agent obj);
static int agent_list_remove_entry(struct agent_list **al_ptr,
				   const char *sink_identifier);
static struct agent_list **agent_search_ptr(struct agent_list **al_ptr,
					    const char *sink_identifier);

static struct agent_list *agent_entry_node;
static struct agent_list *rpc_ag_entry_node;
static const char *local_eid;

void agent_manager_init(const char *const ud3tn_local_eid)
{
	local_eid = ud3tn_local_eid;
}

int agent_register(struct agent agent, const bool is_subscriber)
{
	struct agent_list **const al_ptr = (
		is_subscriber ? &agent_entry_node : &rpc_ag_entry_node
	);
	struct agent_list **const al_ptr_other = (
		is_subscriber ? &rpc_ag_entry_node : &agent_entry_node
	);

	ASSERT(local_eid != NULL && strlen(local_eid) > 3);
	if (get_eid_scheme(local_eid) == EID_SCHEME_IPN) {
		const char *end = parse_ipn_ull(agent.sink_identifier, NULL);

		if (end == NULL || *end != '\0')
			return -1;
	} else if (validate_dtn_eid_demux(agent.sink_identifier) != UD3TN_OK) {
		return -1;
	}

	/* check if agent with that sink_id is already existing */
	if (agent_search(al_ptr, agent.sink_identifier) != NULL) {
		LOGF_WARN(
			"AgentManager: Agent with sink_id %s is already registered!",
			agent.sink_identifier
		);
		return -1;
	}

	struct agent *ag_ex = agent_search(al_ptr_other, agent.sink_identifier);

	if (ag_ex && agent.secret != ag_ex->secret &&
	    strcmp(agent.secret, ag_ex->secret) != 0) {
		LOGF_WARN(
			"AgentManager: Invalid secret provided for sink_id %s!",
			agent.sink_identifier
		);
		return -1;
	}

	if (agent_list_add_entry(al_ptr, agent)) // adding failed
		return -1;

	LOGF_INFO(
		"AgentManager: Agent registered for sink \"%s\"",
		agent.sink_identifier
	);

	return 0;
}

int agent_deregister(const char *sink_identifier, const bool is_subscriber)
{
	struct agent_list **const al_ptr = (
		is_subscriber ? &agent_entry_node : &rpc_ag_entry_node
	);
	struct agent *ag_ptr;

	ag_ptr = agent_search(
		al_ptr,
		sink_identifier
	);

	/* check if agent with that sink_id is not existing */
	if (ag_ptr == NULL) {
		LOGF_WARN(
			"AgentManager: Agent with sink_id %s is not registered!",
			sink_identifier
		);
		return -1;
	}

	if (agent_list_remove_entry(al_ptr, sink_identifier))
		return -1;

	return 0;
}

int agent_forward(const char *sink_identifier, struct bundle_adu data,
		  const void *bp_context)
{
	struct agent *ag_ptr = agent_search(&agent_entry_node, sink_identifier);

	if (ag_ptr == NULL) {
		LOGF_WARN(
			"AgentManager: No agent registered for identifier \"%s\"!",
			sink_identifier
		);
		bundle_adu_free_members(data);
		return -1;
	}

	if (ag_ptr->callback == NULL) {
		LOGF_ERROR(
			"AgentManager: Agent \"%s\" registered, but invalid (null) callback function!",
			sink_identifier
		);
		bundle_adu_free_members(data);
		return -1;
	}
	ag_ptr->callback(data, ag_ptr->param, bp_context);

	return 0;
}

static struct agent_list **agent_search_ptr(struct agent_list **al_ptr,
					    const char *sink_identifier)
{
	/* loop runs until currently examined element is NULL => not found */
	while (*al_ptr) {
		if (!strcmp((*al_ptr)->agent_data.sink_identifier,
			    sink_identifier))
			return al_ptr;
		al_ptr = &(*al_ptr)->next;
	}
	return NULL;
}

struct agent *agent_search(struct agent_list **al_ptr,
			   const char *sink_identifier)
{
	al_ptr = agent_search_ptr(al_ptr, sink_identifier);

	if (al_ptr)
		return &(*al_ptr)->agent_data;
	return NULL;
}

static int agent_list_add_entry(struct agent_list **al_ptr, struct agent obj)
{
	struct agent_list *ag_ptr;
	struct agent_list *ag_iterator;

	if (agent_search(al_ptr, obj.sink_identifier) != NULL) {
		/* entry already existing */
		return -1;
	}

	/* allocate the list struct and make sure next points to NULL */
	ag_ptr = malloc(sizeof(struct agent_list));
	if (ag_ptr == NULL)
		return -1;

	ag_ptr->next = NULL;
	ag_ptr->agent_data = obj;

	/* check if agent list is empty */
	if (*al_ptr == NULL) {
		/* make the new agent node the head of
		 * the list and return success
		 */
		*al_ptr = ag_ptr;
		return 0;
	}

	/* assign the root element to iterator */
	ag_iterator = *al_ptr;

	/* iterate over the linked list until the identifier is found
	 * or the end of the list is reached
	 */
	while (true) {
		if (ag_iterator->next != NULL) {
			ag_iterator = ag_iterator->next;
		} else {
			/* add list element to end of list
			 * (assuming that the list won't get very long,
			 *  additional ordering is not necessary and
			 *  considered an unnecessary overhead)
			 */
			ag_iterator->next = ag_ptr;
			return 0;
		}
	}
}

static int agent_list_remove_entry(struct agent_list **al_ptr,
				   const char *sink_identifier)
{
	al_ptr = agent_search_ptr(al_ptr, sink_identifier);
	struct agent_list *next_element;

	if (!*al_ptr)
		/* entry not existing --> no removal necessary */
		return -1;

	/* perform the removal while keeping the proper pointer value */
	next_element = (*al_ptr)->next;
	free(*al_ptr);
	*al_ptr = next_element;
	return 0;
}
