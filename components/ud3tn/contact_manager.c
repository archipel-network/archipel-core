// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/contact_manager.h"
#include "ud3tn/node.h"
#include "ud3tn/routing_table.h"
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
	QueueIdentifier_t bp_queue;
	struct contact_list **contact_list_ptr;
};

struct contact_info {
	struct contact *contact;
	char *eid;
	char *cla_addr;
};

struct contact_manager_context {
	struct contact_info current_contacts[MAX_CONCURRENT_CONTACTS];
	int8_t current_contact_count;
	uint64_t next_contact_time;
};

static bool contact_active(
	const struct contact_manager_context *const ctx,
	const struct contact *contact)
{
	int8_t i;

	for (i = 0; i < ctx->current_contact_count; i++) {
		if (ctx->current_contacts[i].contact == contact)
			return true;
	}
	return false;
}

static int8_t remove_expired_contacts(
	struct contact_manager_context *const ctx,
	const uint64_t current_timestamp, struct contact_info list[])
{
	/* Check for ending contacts */
	/* We do this first as it frees space in the list */
	int8_t i, c, removed = 0;

	for (i = ctx->current_contact_count - 1; i >= 0; i--) {
		if (ctx->current_contacts[i].contact->to <= current_timestamp) {
			ASSERT(i <= MAX_CONCURRENT_CONTACTS);
			/* Unset "active" constraint */
			ctx->current_contacts[i].contact->active = 0;
			/* The TX task takes care of re-scheduling */
			list[removed++] = ctx->current_contacts[i];
			/* If it's not the last element, we have to move mem */
			if (i != ctx->current_contact_count - 1) {
				for (c = i; c < ctx->current_contact_count; c++)
					ctx->current_contacts[c] =
						ctx->current_contacts[c+1];
			}
			ctx->current_contact_count--;
		}
	}
	return removed;
}

static uint8_t check_upcoming(
	struct contact_manager_context *const ctx,
	struct contact *c, struct contact_info list[], const uint8_t index)
{
	// Contact is already active, do nothing
	if (contact_active(ctx, c))
		return 0;

	// Too many contacts are already active, cannot add another...
	if (ctx->current_contact_count >= MAX_CONCURRENT_CONTACTS) {
		LOGF("ContactManager: Cannot start contact with \"%s\", too many contacts are already active",
		    c->node->eid);
		return 0;
	}

	/* Set "active" constraint, "blocking" the contact */
	c->active = 1;

	/* Add contact */
	ctx->current_contacts[ctx->current_contact_count].contact = c;
	ctx->current_contacts[ctx->current_contact_count].eid = strdup(
		c->node->eid
	);
	if (!ctx->current_contacts[ctx->current_contact_count].eid) {
		LOG("ContactManager: Failed to copy EID");
		return 0;
	}
	ctx->current_contacts[ctx->current_contact_count].cla_addr = strdup(
		c->node->cla_addr
	);
	if (!ctx->current_contacts[ctx->current_contact_count].cla_addr) {
		LOG("ContactManager: Failed to copy CLA address");
		free(ctx->current_contacts[ctx->current_contact_count].eid);
		return 0;
	}
	list[index] = ctx->current_contacts[ctx->current_contact_count];
	ctx->current_contact_count++;

	return 1;
}

static int8_t process_upcoming_list(
	struct contact_manager_context *const ctx,
	struct contact_list *contact_list, const uint64_t current_timestamp,
	struct contact_info list[])
{
	int8_t added = 0;
	struct contact_list *cur_entry;

	ctx->next_contact_time = UINT64_MAX;
	cur_entry = contact_list;
	while (cur_entry != NULL) {
		if (cur_entry->data->from <= current_timestamp) {
			if (cur_entry->data->to > current_timestamp) {
				added += check_upcoming(
					ctx,
					cur_entry->data,
					list,
					added
				);
				if (cur_entry->data->to < ctx->next_contact_time)
					ctx->next_contact_time =
						cur_entry->data->to;
			}
		} else {
			if (cur_entry->data->from < ctx->next_contact_time)
				ctx->next_contact_time = cur_entry->data->from;
			/* As our contact_list is sorted ascending by */
			/* from-time we can stop checking here */
			break;
		}
		cur_entry = cur_entry->next;
	}
	return added;
}

static int hand_over_contact_bundles(
	struct contact_manager_context *const ctx, Semaphore_t semphr, int8_t i)
{
	struct contact_info cinfo = ctx->current_contacts[i];

	hal_semaphore_take_blocking(semphr);

	// NOTE: cinfo.contact MAY not be valid at this point!
	struct node_table_entry *n = routing_table_lookup_eid(cinfo.eid);
	struct contact_list *cl = (n != NULL) ? n->contacts : NULL;
	bool found = false;

	while (cl) {
		if (cl->data == cinfo.contact) {
			found = true;
			break;
		}
		cl = cl->next;
	}

	if (!found) {
		LOGF("ContactManager: Could not find contact %p to \"%s\" via \"%s\", discarding record",
		     cinfo.contact, cinfo.eid, cinfo.cla_addr);
		// Remove invalid contact info
		free(cinfo.eid);
		free(cinfo.cla_addr);
		if (i < ctx->current_contact_count - 1) {
			memmove(
				&ctx->current_contacts[i],
				&ctx->current_contacts[i + 1],
				sizeof(struct contact_info) * (
					ctx->current_contact_count - 1 - i
				)
			);
		}
		ctx->current_contact_count--;
		hal_semaphore_release(semphr);
		return 0;
	}

	// Contact found and valid -> continue!
	if (cinfo.contact->contact_bundles == NULL) {
		hal_semaphore_release(semphr);
		return 1;
	}

	ASSERT(cinfo.cla_addr != NULL);
	// Try to obtain a handler
	struct cla_config *cla_config = cla_config_get(cinfo.cla_addr);

	if (!cla_config) {
		LOGF("ContactManager: Could not obtain CLA for address \"%s\"",
		     cinfo.cla_addr);
		hal_semaphore_release(semphr);
		return 1;
	}

	struct cla_tx_queue tx_queue = cla_config->vtable->cla_get_tx_queue(
		cla_config,
		cinfo.eid,
		cinfo.cla_addr
	);

	if (!tx_queue.tx_queue_handle) {
		LOGF("ContactManager: Could not obtain queue for TX to \"%s\" via \"%s\"",
		     cinfo.eid, cinfo.cla_addr);
		// Re-scheduling will be done by routerTask or transmission will
		// occur after signal of new connection.
		hal_semaphore_release(semphr);
		return 1;
	}

	LOGF("ContactManager: Queuing bundles for contact with \"%s\".",
	     cinfo.eid);

	struct cla_contact_tx_task_command command = {
		.type = TX_COMMAND_BUNDLES,
		// Take over the bundles as we can now push them into the queue
		// that is protected by the CLA semaphore.
		.bundles = cinfo.contact->contact_bundles,
	};

	// Ensure the Router does not interfere. We own the list now and the
	// TX task will free it.
	cinfo.contact->contact_bundles = NULL;
	// Now we can also let the BP do its thing again...
	hal_semaphore_release(semphr);
	// NOTE: From now on, cinfo.contact MAY become invalid again!

	command.cla_address = strdup(cinfo.cla_addr);
	hal_queue_push_to_back(tx_queue.tx_queue_handle, &command);
	hal_semaphore_release(tx_queue.tx_queue_sem); // taken by get_tx_queue

	return 1;
}

static uint8_t check_for_contacts(
	struct contact_manager_context *const ctx,
	struct contact_list *contact_list,
	struct contact_info removed_contacts[])
{
	int8_t i;
	static struct contact_info added_contacts[MAX_CONCURRENT_CONTACTS];
	uint64_t current_timestamp = hal_time_get_timestamp_s();
	int8_t removed_count = remove_expired_contacts(
		ctx,
		current_timestamp,
		removed_contacts
	);
	int8_t added_count = process_upcoming_list(
		ctx,
		contact_list,
		current_timestamp,
		added_contacts
	);

	for (i = 0; i < added_count; i++) {
		LOGF("ContactManager: Scheduled contact with \"%s\" started (%p).",
		     added_contacts[i].eid,
		     added_contacts[i].contact);

		struct cla_config *cla_config = cla_config_get(
			added_contacts[i].cla_addr
		);

		if (!cla_config) {
			LOGF("ContactManager: Could not obtain CLA for address \"%s\"",
			     added_contacts[i].cla_addr);
		} else {
			cla_config->vtable->cla_start_scheduled_contact(
				cla_config,
				added_contacts[i].eid,
				added_contacts[i].cla_addr
			);
		}
	}
	for (i = 0; i < removed_count; i++) {
		LOGF("ContactManager: Scheduled contact with \"%s\" ended (%p).",
		     removed_contacts[i].eid,
		     removed_contacts[i].contact);

		struct cla_config *cla_config = cla_config_get(
			removed_contacts[i].cla_addr
		);

		if (!cla_config) {
			LOGF("ContactManager: Could not obtain CLA for address \"%s\"",
			     removed_contacts[i].cla_addr);
		} else {
			cla_config->vtable->cla_end_scheduled_contact(
				cla_config,
				removed_contacts[i].eid,
				removed_contacts[i].cla_addr
			);
		}
		free(removed_contacts[i].eid);
		free(removed_contacts[i].cla_addr);
	}
	return removed_count;
}

/* We assume that contact_list will not change. */
static void manage_contacts(
	struct contact_manager_context *const ctx,
	struct contact_list **contact_list, enum contact_manager_signal signal,
	Semaphore_t semphr, QueueIdentifier_t bp_queue)
{
	struct contact_info removed_list[MAX_CONCURRENT_CONTACTS];
	int8_t removed, i;

	ASSERT(semphr != NULL);
	ASSERT(bp_queue != NULL);

	// NOTE: CM_SIGNAL_UNKNOWN has both flags
	if (HAS_FLAG(signal, CM_SIGNAL_UPDATE_CONTACT_LIST)) {
		hal_semaphore_take_blocking(semphr);
		removed = check_for_contacts(ctx, *contact_list, removed_list);
		hal_semaphore_release(semphr);
		for (i = 0; i < removed; i++) {
			/* The contact has to be deleted first... */
			bundle_processor_inform(
				bp_queue,
				NULL,
				BP_SIGNAL_CONTACT_OVER,
				NULL,
				NULL,
				removed_list[i].contact,
				NULL
			);
		}
	}

	// NOTE: CM_SIGNAL_UNKNOWN has both flags
	if (HAS_FLAG(signal, CM_SIGNAL_PROCESS_CURRENT_BUNDLES)) {
		for (int8_t i = 0; i < ctx->current_contact_count; ) {
			// NOTE this may either return 1 or 0, the latter if it
			// deleted an item & modified ctx->current_contact_count
			i += hand_over_contact_bundles(ctx, semphr, i);
		}
	}
}

static void contact_manager_task(void *cm_parameters)
{
	struct contact_manager_task_parameters *parameters
		= (struct contact_manager_task_parameters *)cm_parameters;
	int8_t led_state = 0;
	enum contact_manager_signal signal = CM_SIGNAL_NONE;
	uint64_t cur_time, next_time;
	int32_t delay;
	struct contact_manager_context ctx = {
		.current_contact_count = 0,
		.next_contact_time = UINT64_MAX,
	};

	if (!parameters) {
		LOG("ContactManager: Cannot start, parameters not defined");
		ASSERT(false);
		return;
	}
	for (;;) {
		hal_platform_led_set((led_state = 1 - led_state) + 3);

		if (signal != CM_SIGNAL_NONE) {
			manage_contacts(
				&ctx,
				parameters->contact_list_ptr,
				signal,
				parameters->semaphore,
				parameters->bp_queue
			);
		}
		signal = CM_SIGNAL_UNKNOWN;
		cur_time = hal_time_get_timestamp_ms();
		delay = -1; // infinite blocking on queue
		if (ctx.next_contact_time < UINT64_MAX / 1000) {
			next_time = ctx.next_contact_time * 1000;
			if (next_time <= cur_time)
				continue;
			if (next_time - cur_time < (uint64_t)INT32_MAX)
				delay = next_time - cur_time + 1;
		}
		hal_queue_receive(
			parameters->control_queue,
			&signal,
			delay
		);
	}
}

struct contact_manager_params contact_manager_start(
	QueueIdentifier_t bp_queue,
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
	cmt_params->bp_queue = bp_queue;
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
