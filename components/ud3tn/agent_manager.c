#include "ud3tn/agent_manager.h"
#include "ud3tn/bundle.h"

#include "agents/config_agent.h"
#include "agents/application_agent.h"

#include "platform/hal_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static struct agent *agent_search(const char *sink_identifier);
static int agent_list_add_entry(struct agent *obj);
static int agent_list_remove_entry(struct agent *obj);
static struct agent_list **agent_search_ptr(const char *sink_identifier);

static struct agent_list *agent_entry_node;

int agent_register(const char *sink_identifier,
		   void (*const callback)(struct bundle_adu data, void *param),
		   void *param)
{
	struct agent *ag_ptr;

	/* check if agent with that sink_id is already existing */
	if (agent_search(sink_identifier) != NULL) {
		LOGF(
			"AgentManager: Agent with sink_id %s is already registered! Abort!",
			sink_identifier);
		return -1;
	}

	ag_ptr = malloc(sizeof(struct agent));
	if (!ag_ptr)
		return -1;

	ag_ptr->sink_identifier = sink_identifier;
	ag_ptr->callback = callback;
	ag_ptr->param = param;

	if (agent_list_add_entry(ag_ptr)) {
		/* the adding process to the list failed */
		free(ag_ptr);
		return -1;
	}

	LOGF("AgentManager: Agent registered for sink \"%s\"",
			     sink_identifier);

	return 0;
}

int agent_deregister(const char *sink_identifier)
{
	struct agent *ag_ptr;

	ag_ptr = agent_search(sink_identifier);

	/* check if agent with that sink_id is not existing */
	if (ag_ptr == NULL) {
		LOGF(
		     "AgentManager: Agent with sink_id %s is not registered! Abort!",
		     sink_identifier);
		return -1;
	}

	if (agent_list_remove_entry(ag_ptr)) {
		/* the adding process to the list failed */
		free(ag_ptr);
		return -1;
	}

	free(ag_ptr);
	return 0;
}

int agent_forward(const char *sink_identifier, struct bundle_adu data)
{
	struct agent *ag_ptr = agent_search(sink_identifier);

	if (ag_ptr == NULL) {
		LOGF("AgentManager: No agent registered for identifier \"%s\"!",
				     sink_identifier);
		bundle_adu_free_members(data);
		return -1;
	}

	if (ag_ptr->callback == NULL) {
		LOGF(
		     "AgentManager: Agent \"%s\" registered, but invalid (null) callback function!",
		     sink_identifier);
		bundle_adu_free_members(data);
		return -1;
	}
	ag_ptr->callback(data, ag_ptr->param);

	return 0;
}

static struct agent_list **agent_search_ptr(const char *sink_identifier)
{
	struct agent_list **al_ptr = &agent_entry_node;

	/* loop runs until currently examined element is NULL => not found */
	while (*al_ptr) {
		if (!strcmp((*al_ptr)->agent_data->sink_identifier,
			    sink_identifier))
			return al_ptr;
		al_ptr = &(*al_ptr)->next;
	}
	return NULL;
}

struct agent *agent_search(const char *sink_identifier)
{
	struct agent_list **al_ptr = agent_search_ptr(sink_identifier);

	if (al_ptr)
		return (*al_ptr)->agent_data;
	return NULL;
}

static int agent_list_add_entry(struct agent *obj)
{
	struct agent_list *ag_ptr;
	struct agent_list *ag_iterator;

	if (agent_search(obj->sink_identifier) != NULL) {
		/* entry already existing */
		return -1;
	}

	/* allocate the list struct and make sure next points to NULL */
	ag_ptr = malloc(sizeof(struct agent_list));
	ag_ptr->next = NULL;
	ag_ptr->agent_data = obj;

	/* check if agent_entry is existing at all */
	if (agent_entry_node == NULL) {
		/* set list object as new agent_entry node and
		 * return success
		 */
		agent_entry_node = ag_ptr;
		return 0;
	}

	/* assign the root element to iterator */
	ag_iterator = agent_entry_node;

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

	/* something went wrong */
	return -1;
}

static int agent_list_remove_entry(struct agent *obj)
{
	struct agent_list **al_ptr = agent_search_ptr(obj->sink_identifier);
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
