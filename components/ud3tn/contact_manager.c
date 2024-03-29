// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/common.h"
#include "ud3tn/contact_manager.h"
#include "ud3tn/node.h"
#include "ud3tn/routing_table.h"
#include "archipel-core/bundle_restore.h"

#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"

#include "platform/hal_io.h"
#include "platform/hal_platform.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"
#include "platform/hal_time.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


struct contact_manager_task_parameters {
	Semaphore_t semaphore;
	QueueIdentifier_t control_queue;
	QueueIdentifier_t bp_queue;
	struct contact_list **contact_list_ptr;
	#ifdef ARCHIPEL_CORE
	QueueIdentifier_t restore_queue;
	#endif
};

struct contact_info {
	struct contact *contact;
	char *eid;
	char *cla_addr;
};

struct contact_manager_context {
	struct contact_info current_contacts[MAX_CONCURRENT_CONTACTS];
	int8_t current_contact_count;
	uint64_t next_contact_time_ms;
	#ifdef ARCHIPEL_CORE
	QueueIdentifier_t bundle_restore_queue;
	#endif
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
	const uint64_t current_timestamp_ms, struct contact_info list[])
{
	/* Check for ending contacts */
	/* We do this first as it frees space in the list */
	int8_t i, c, removed = 0;

	for (i = ctx->current_contact_count - 1; i >= 0; i--) {
		if (ctx->current_contacts[i].contact->to_ms <=
		    current_timestamp_ms) {
			ASSERT(i <= MAX_CONCURRENT_CONTACTS);
			/* Unset "active" constraint */
			ctx->current_contacts[i].contact->active = 0;
			/* The TX task takes care of re-scheduling */
			list[removed++] = ctx->current_contacts[i];
			/* If it's not the last element, we have to move mem */
			if (i != ctx->current_contact_count - 1) {
				for (c = i; c < ctx->current_contact_count; c++)
					ctx->current_contacts[c] =
						ctx->current_contacts[c + 1];
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
		LOGF_WARN(
			"ContactManager: Cannot start contact with \"%s\", too many contacts are already active",
			c->node->eid
		);
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
		LOG_ERROR("ContactManager: Failed to copy EID");
		return 0;
	}
	ctx->current_contacts[ctx->current_contact_count].cla_addr = strdup(
		c->node->cla_addr
	);
	if (!ctx->current_contacts[ctx->current_contact_count].cla_addr) {
		LOG_ERROR("ContactManager: Failed to copy CLA address");
		free(ctx->current_contacts[ctx->current_contact_count].eid);
		return 0;
	}
	list[index] = ctx->current_contacts[ctx->current_contact_count];
	ctx->current_contact_count++;

	return 1;
}

static int8_t process_upcoming_list(
	struct contact_manager_context *const ctx,
	struct contact_list *contact_list, const uint64_t current_timestamp_ms,
	struct contact_info list[])
{
	int8_t added = 0;
	struct contact_list *cur_entry;

	ctx->next_contact_time_ms = UINT64_MAX;
	cur_entry = contact_list;
	while (cur_entry != NULL) {
		if (cur_entry->data->from_ms <= current_timestamp_ms) {
			if (cur_entry->data->to_ms > current_timestamp_ms) {
				added += check_upcoming(
					ctx,
					cur_entry->data,
					list,
					added
				);
				if (cur_entry->data->to_ms <
				    ctx->next_contact_time_ms)
					ctx->next_contact_time_ms =
						cur_entry->data->to_ms;
			}
		} else {
			if (cur_entry->data->from_ms <
			    ctx->next_contact_time_ms)
				ctx->next_contact_time_ms = (
					cur_entry->data->from_ms
				);
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
		LOGF_WARN(
			"ContactManager: Could not find contact %p to \"%s\" via \"%s\", discarding record",
			cinfo.contact,
			cinfo.eid,
			cinfo.cla_addr
		);
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
		LOGF_WARN(
			"ContactManager: Could not obtain CLA for address \"%s\"",
			cinfo.cla_addr
		);
		hal_semaphore_release(semphr);
		return 1;
	}

	struct cla_tx_queue tx_queue = cla_config->vtable->cla_get_tx_queue(
		cla_config,
		cinfo.eid,
		cinfo.cla_addr
	);

	if (!tx_queue.tx_queue_handle) {
		LOGF_WARN(
			"ContactManager: Could not obtain queue for TX to \"%s\" via \"%s\"",
			cinfo.eid,
			cinfo.cla_addr
		);
		// Re-scheduling will be done by routerTask or transmission will
		// occur after signal of new connection.
		hal_semaphore_release(semphr);
		return 1;
	}

	LOGF_INFO(
		"ContactManager: Queuing bundles for contact with \"%s\".",
		cinfo.eid
	);

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
	const uint64_t current_timestamp_ms = hal_time_get_timestamp_ms();
	const int8_t removed_count = remove_expired_contacts(
		ctx,
		current_timestamp_ms,
		removed_contacts
	);
	const int8_t added_count = process_upcoming_list(
		ctx,
		contact_list,
		current_timestamp_ms,
		added_contacts
	);

	ASSERT(ctx->next_contact_time_ms > current_timestamp_ms);

	for (i = 0; i < added_count; i++) {
		LOGF_INFO(
			"ContactManager: Scheduled contact with \"%s\" started (%p).",
			added_contacts[i].eid,
			added_contacts[i].contact
		);

		struct cla_config *cla_config = cla_config_get(
			added_contacts[i].cla_addr
		);

		if (!cla_config) {
			LOGF_WARN(
				"ContactManager: Could not obtain CLA for address \"%s\"",
				added_contacts[i].cla_addr
			);
		} else {
			cla_config->vtable->cla_start_scheduled_contact(
				cla_config,
				added_contacts[i].eid,
				added_contacts[i].cla_addr
			);
		}

		#ifdef ARCHIPEL_CORE
		bundle_restore_for_destination(
			ctx->bundle_restore_queue,
			added_contacts[i].eid);
		#endif
	}
	for (i = 0; i < removed_count; i++) {
		LOGF_INFO(
			"ContactManager: Scheduled contact with \"%s\" ended (%p).",
			removed_contacts[i].eid,
			removed_contacts[i].contact
		);

		struct cla_config *cla_config = cla_config_get(
			removed_contacts[i].cla_addr
		);

		if (!cla_config) {
			LOGF_WARN(
				"ContactManager: Could not obtain CLA for address \"%s\"",
				removed_contacts[i].cla_addr
			);
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
				(struct bundle_processor_signal) {
					.type = BP_SIGNAL_CONTACT_OVER,
					.contact = removed_list[i].contact,
				}
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
	struct contact_manager_task_parameters *parameters =
		(struct contact_manager_task_parameters *)cm_parameters;
	enum contact_manager_signal signal = CM_SIGNAL_NONE;
	uint64_t cur_time_ms, next_time_ms;
	int64_t delay_ms;
	struct contact_manager_context ctx = {
		.current_contact_count = 0,
		.next_contact_time_ms = UINT64_MAX,
		#ifdef ARCHIPEL_CORE
		.bundle_restore_queue = parameters->restore_queue
		#endif
	};

	if (!parameters) {
		LOG_ERROR("ContactManager: Cannot start, parameters not defined");
		abort();
	}
	for (;;) {
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
		cur_time_ms = hal_time_get_timestamp_ms();
		delay_ms = -1; // infinite blocking on queue
		if (ctx.next_contact_time_ms < UINT64_MAX) {
			next_time_ms = ctx.next_contact_time_ms;
			if (next_time_ms <= cur_time_ms)
				continue;

			const uint64_t udelay = next_time_ms - cur_time_ms + 1;

			// The queue implementation does not support to wait
			// longer than 292 years due to a conversion to
			// nanoseconds. Block indefinitely in this case.
			if (udelay < HAL_QUEUE_MAX_DELAY_MS &&
					udelay <= (uint64_t)INT64_MAX)
				delay_ms = udelay;
		}
		hal_queue_receive(
			parameters->control_queue,
			&signal,
			delay_ms
		);
	}
}

struct contact_manager_params contact_manager_start(
	QueueIdentifier_t bp_queue,
	struct contact_list **clistptr
	#ifdef ARCHIPEL_CORE
	,QueueIdentifier_t bundle_restore_queue
	#endif
	)
{
	struct contact_manager_params ret = {
		.task_creation_result = UD3TN_FAIL,
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
	#ifdef ARCHIPEL_CORE
	cmt_params->restore_queue = bundle_restore_queue;
	#endif
	ret.task_creation_result = hal_task_create(
		contact_manager_task,
		cmt_params
	);
	if (ret.task_creation_result == UD3TN_OK) {
		ret.semaphore = semaphore;
		ret.control_queue = queue;
	} else {
		hal_semaphore_delete(semaphore);
		hal_queue_delete(queue);
	}
	return ret;
}
