#include "ud3tn/common.h"
#include "ud3tn/contact_manager.h"
#include "ud3tn/router_task.h"
#include "ud3tn/node.h"
#include "ud3tn/task_tags.h"

#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_platform.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


struct contact_manager_task_parameters {
	Semaphore_t semaphore;
	QueueIdentifier_t control_queue;
	QueueIdentifier_t router_signaling_queue;
	struct contact_list **contact_list_ptr;
};

struct contact_info {
	struct contact *contact;
	struct cla_config *cla_conf;
	char *eid;
	char *cla_addr;
};

static struct contact_info current_contacts[MAX_CONCURRENT_CONTACTS];
static int8_t current_contact_count;
static uint64_t next_contact_time = UINT64_MAX;

static bool contact_active(const struct contact *contact)
{
	int8_t i;

	for (i = 0; i < current_contact_count; i++) {
		if (current_contacts[i].contact == contact)
			return true;
	}
	return false;
}

static int8_t remove_expired_contacts(
	const uint64_t current_timestamp, struct contact_info list[])
{
	/* Check for ending contacts */
	/* We do this first as it frees space in the list */
	int8_t i, c, removed = 0;

	for (i = current_contact_count - 1; i >= 0; i--) {
		if (current_contacts[i].contact->to <= current_timestamp) {
			ASSERT(i <= MAX_CONCURRENT_CONTACTS);
			/* Unset "active" constraint */
			current_contacts[i].contact->active = 0;
			/* The TX task takes care of re-scheduling */
			list[removed++] = current_contacts[i];
			/* If it's not the last element, we have to move mem */
			if (i != current_contact_count - 1) {
				for (c = i; c < current_contact_count; c++)
					current_contacts[c] =
						current_contacts[c+1];
			}
			current_contact_count--;
		}
	}
	return removed;
}

static uint8_t check_upcoming(
	struct contact *c, struct contact_info list[], const uint8_t index)
{
	// Contact is already active, do nothing
	if (contact_active(c))
		return 0;

	// Too many contacts are already active, cannot add another...
	if (current_contact_count >= MAX_CONCURRENT_CONTACTS)
		return 0;

	/* Set "active" constraint, "blocking" the contact */
	c->active = 1;

	ASSERT(c->node->cla_addr != NULL);
	// Try to obtain a handler
	struct cla_config *cla_config = cla_config_get(
		c->node->cla_addr
	);

	if (!cla_config) {
		LOGF("ContactManager: Could not obtain CLA for address \"%s\"",
		     c->node->cla_addr);
		return 0;
	}

	/* Add contact */
	current_contacts[current_contact_count].contact = c;
	current_contacts[current_contact_count].cla_conf = cla_config;
	current_contacts[current_contact_count].eid = strdup(
		c->node->eid
	);
	if (!current_contacts[current_contact_count].eid) {
		LOG("ContactManager: Failed to copy EID");
		return 0;
	}
	current_contacts[current_contact_count].cla_addr = strdup(
		c->node->cla_addr
	);
	if (!current_contacts[current_contact_count].cla_addr) {
		LOG("ContactManager: Failed to copy CLA address");
		free(current_contacts[current_contact_count].eid);
		return 0;
	}
	list[index] = current_contacts[current_contact_count];
	current_contact_count++;

	return 1;
}

static int8_t process_upcoming_list(
	struct contact_list *contact_list, const uint64_t current_timestamp,
	struct contact_info list[])
{
	int8_t added = 0;
	struct contact_list *cur_entry;

	next_contact_time = UINT64_MAX;
	cur_entry = contact_list;
	while (cur_entry != NULL) {
		if (cur_entry->data->from <= current_timestamp) {
			if (cur_entry->data->to > current_timestamp) {
				added += check_upcoming(
					cur_entry->data, list, added);
				if (cur_entry->data->to < next_contact_time)
					next_contact_time = cur_entry->data->to;
			}
		} else {
			if (cur_entry->data->from < next_contact_time)
				next_contact_time = cur_entry->data->from;
			/* As our contact_list is sorted ascending by */
			/* from-time we can stop checking here */
			break;
		}
		cur_entry = cur_entry->next;
	}
	return added;
}

static void hand_over_contact_bundles(struct contact_info c)
{
	struct cla_contact_tx_task_command command = {
		.type = TX_COMMAND_BUNDLES,
		.bundles = NULL,
	};

	ASSERT(c.contact != NULL);
	if (c.contact->contact_bundles == NULL)
		return;

	struct cla_tx_queue tx_queue = c.cla_conf->vtable->cla_get_tx_queue(
		c.cla_conf,
		c.eid,
		c.cla_addr
	);

	if (!tx_queue.tx_queue_handle) {
		LOGF("ContactManager: Could not obtain queue for TX to \"%s\" via \"%s\"",
		     c.eid, c.cla_addr);
		// Re-scheduling will be done by routerTask or transmission will
		// occur after signal of new connection.
		return;
	}

	LOGF("ContactManager: Queuing bundles for contact with \"%s\".", c.eid);

	command.bundles = c.contact->contact_bundles;
	c.contact->contact_bundles = NULL;
	hal_queue_push_to_back(tx_queue.tx_queue_handle, &command);
	hal_semaphore_release(tx_queue.tx_queue_sem); // taken by get_tx_queue
}

static void process_current_contacts(void)
{
	int8_t i;

	for (i = 0; i < current_contact_count; i++)
		hand_over_contact_bundles(current_contacts[i]);
}

static uint8_t check_for_contacts(struct contact_list *contact_list,
	struct contact_info removed_contacts[])
{
	int8_t i;
	static struct contact_info added_contacts[MAX_CONCURRENT_CONTACTS];
	uint64_t current_timestamp = hal_time_get_timestamp_s();
	int8_t removed_count = remove_expired_contacts(
		current_timestamp,
		removed_contacts
	);
	int8_t added_count = process_upcoming_list(
		contact_list,
		current_timestamp,
		added_contacts
	);

	for (i = 0; i < added_count; i++) {
		LOGF("ContactManager: Scheduled contact with \"%s\" started (%p).",
		     added_contacts[i].eid,
		     added_contacts[i].contact);
		// TODO: Handle errors this may return in case of TCPCL
		added_contacts[i].cla_conf->vtable->cla_start_scheduled_contact(
			added_contacts[i].cla_conf,
			added_contacts[i].eid,
			added_contacts[i].cla_addr
		);
		hand_over_contact_bundles(added_contacts[i]);
	}
	for (i = 0; i < removed_count; i++) {
		LOGF("ContactManager: Scheduled contact with \"%s\" ended (%p).",
		     removed_contacts[i].eid,
		     removed_contacts[i].contact);
		removed_contacts[i].cla_conf->vtable->cla_end_scheduled_contact(
			removed_contacts[i].cla_conf,
			removed_contacts[i].eid,
			removed_contacts[i].cla_addr
		);
		free(removed_contacts[i].eid);
		free(removed_contacts[i].cla_addr);
	}
	return removed_count;
}

static void inform_router(
	enum router_signal_type type, void *data, QueueIdentifier_t queue)
{
	struct router_signal signal = {
		.type = type,
		.data = data
	};

	ASSERT(data != NULL);
	hal_queue_push_to_back(queue, &signal);
}

/* We assume that contact_list will not change. */
static void manage_contacts(
	struct contact_list **contact_list, enum contact_manager_signal signal,
	Semaphore_t semphr, QueueIdentifier_t queue)
{
	static struct contact_info rem[MAX_CONCURRENT_CONTACTS];
	int8_t removed, i;

	ASSERT(semphr != NULL);
	ASSERT(queue != NULL);
	hal_semaphore_take_blocking(semphr);
	if (HAS_FLAG(signal, CM_SIGNAL_PROCESS_CURRENT_BUNDLES))
		process_current_contacts();
	/* If there only was a new bundle, skip further checks */
	if (signal == CM_SIGNAL_PROCESS_CURRENT_BUNDLES) {
		hal_semaphore_release(semphr);
		return;
	}
	removed = check_for_contacts(*contact_list, rem);
	hal_semaphore_release(semphr);
	for (i = 0; i < removed; i++) {
		/* The contact has to be deleted first... */
		inform_router(ROUTER_SIGNAL_CONTACT_OVER,
			rem[i].contact, queue);
	}
}

static void contact_manager_task(void *cm_parameters)
{
	struct contact_manager_task_parameters *parameters
		= (struct contact_manager_task_parameters *)cm_parameters;
	int8_t led_state = 0;
	enum contact_manager_signal signal = CM_SIGNAL_NONE;
	uint64_t cur_time, next_time, delay;

	ASSERT(parameters != NULL);
	for (;;) {
		hal_platform_led_set((led_state = 1 - led_state) + 3);

		if (signal != CM_SIGNAL_NONE) { /* TODO: end by query to BP */
			manage_contacts(parameters->contact_list_ptr, signal,
				parameters->semaphore,
				parameters->router_signaling_queue);
		}
		signal = CM_SIGNAL_UNKNOWN;
		cur_time = hal_time_get_timestamp_ms();

		next_time = MIN(UINT64_MAX, next_contact_time * 1000);
		if (next_time > (cur_time + CONTACT_CHECKING_MAX_PERIOD))
			delay = CONTACT_CHECKING_MAX_PERIOD;
		else if (next_time <= cur_time)
			continue;
		else
			delay = next_time - cur_time;
		/*LOGA("ContactManager: Suspended on queue.",*/
		/*	(uint8_t)(delay / 1000), LOG_NO_ITEM);*/
		hal_queue_receive(parameters->control_queue, &signal,
			delay+1);
	}
}

struct contact_manager_params contact_manager_start(
	QueueIdentifier_t router_signaling_queue,
	struct contact_list **clistptr)
{
	struct contact_manager_params ret = {
		.semaphore = NULL,
		.control_queue = NULL,
	};
	QueueIdentifier_t queue;
	struct contact_manager_task_parameters *cmt_params;
	Semaphore_t semaphore = hal_semaphore_init_binary();

	if (semaphore == NULL)
		return ret;
	hal_semaphore_release(semaphore);
	queue = hal_queue_create(1, sizeof(enum contact_manager_signal));
	if (queue == NULL) {
		hal_semaphore_delete(semaphore);
		return ret;
	}
	cmt_params = malloc(sizeof(struct contact_manager_task_parameters));
	if (cmt_params == NULL) {
		hal_semaphore_delete(semaphore);
		hal_queue_delete(queue);
		return ret;
	}
	cmt_params->semaphore = semaphore;
	cmt_params->control_queue = queue;
	cmt_params->router_signaling_queue = router_signaling_queue;
	cmt_params->contact_list_ptr = clistptr;
	hal_task_create(contact_manager_task,
			"cont_man_t",
			CONTACT_MANAGER_TASK_PRIORITY,
			cmt_params,
			CONTACT_MANAGER_TASK_STACK_SIZE,
			(void *)CONTACT_MANAGER_TASK_TAG);
	ret.semaphore = semaphore;
	ret.control_queue = queue;
	return ret;
}

uint64_t contact_manager_get_next_contact_time(void)
{
	return next_contact_time;
}

uint8_t contact_manager_in_contact(void)
{
	return current_contact_count != 0;
}

void contact_manager_reset_time(void)
{
	next_contact_time = hal_time_get_timestamp_s();
}
