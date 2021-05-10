#include "ud3tn/bundle.h"
#include "ud3tn/bundle_fragmenter.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/bundle_storage_manager.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/contact_manager.h"
#include "ud3tn/node.h"
#include "ud3tn/router.h"
#include "ud3tn/router_optimizer.h"
#include "ud3tn/router_task.h"
#include "ud3tn/routing_table.h"
#include "ud3tn/task_tags.h"

#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool process_signal(
	struct router_signal signal,
	QueueIdentifier_t bp_signaling_queue,
	QueueIdentifier_t router_signaling_queue,
	Semaphore_t cm_semaphore,
	QueueIdentifier_t cm_queue,
	Semaphore_t ro_sem);

static bool process_router_command(
	struct router_command *router_cmd,
	QueueIdentifier_t bp_signaling_queue);

struct bundle_processing_result {
	int32_t status_or_fragments;
	bundleid_t fragment_ids[ROUTER_MAX_FRAGMENTS];
};

#define BUNDLE_RESULT_NO_ROUTE 0
#define BUNDLE_RESULT_NO_TIMELY_CONTACTS -1
#define BUNDLE_RESULT_NO_MEMORY -2
#define BUNDLE_RESULT_INVALID -3
#define BUNDLE_RESULT_EXPIRED -4

static struct bundle_processing_result process_bundle(struct bundle *bundle);

void router_task(void *rt_parameters)
{
	struct router_task_parameters *parameters;
	struct contact_manager_params cm_param;
	Semaphore_t ro_sem;
	struct router_signal signal;

	ASSERT(rt_parameters != NULL);
	parameters = (struct router_task_parameters *)rt_parameters;

	/* Init routing tables */
	ASSERT(routing_table_init() == UD3TN_OK);
	/* Start contact manager */
	cm_param = contact_manager_start(
		parameters->router_signaling_queue,
		routing_table_get_raw_contact_list_ptr());
	ASSERT(cm_param.control_queue != NULL);
	/* Start optimizer */
	ro_sem = router_start_optimizer_task(
		parameters->router_signaling_queue, cm_param.semaphore,
		routing_table_get_raw_contact_list_ptr());
	ASSERT(ro_sem != NULL);

	for (;;) {
		if (hal_queue_receive(
			parameters->router_signaling_queue, &signal,
			-1) == UD3TN_OK
		) {
			process_signal(signal,
				parameters->bundle_processor_signaling_queue,
				parameters->router_signaling_queue,
				cm_param.semaphore, cm_param.control_queue,
			  ro_sem);
		}
	}
}

static inline enum bundle_processor_signal_type get_bp_signal(int8_t bh_result)
{
	switch (bh_result) {
	case BUNDLE_RESULT_EXPIRED:
		return BP_SIGNAL_BUNDLE_EXPIRED;
	default:
		return BP_SIGNAL_FORWARDING_CONTRAINDICATED;
	}
}

static inline enum bundle_status_report_reason get_reason(int8_t bh_result)
{
	switch (bh_result) {
	case BUNDLE_RESULT_NO_ROUTE:
		return BUNDLE_SR_REASON_NO_KNOWN_ROUTE;
	case BUNDLE_RESULT_NO_MEMORY:
		return BUNDLE_SR_REASON_DEPLETED_STORAGE;
	case BUNDLE_RESULT_EXPIRED:
		return BUNDLE_SR_REASON_LIFETIME_EXPIRED;
	case BUNDLE_RESULT_NO_TIMELY_CONTACTS:
	default:
		return BUNDLE_SR_REASON_NO_TIMELY_CONTACT;
	}
}

static void wake_up_contact_manager(QueueIdentifier_t cm_queue,
				    enum contact_manager_signal cm_signal)
{
	if (hal_queue_try_push_to_back(cm_queue, &cm_signal, 0) == UD3TN_FAIL) {
		// To be safe we let the CM re-check everything in this case.
		cm_signal = (
			CM_SIGNAL_UPDATE_CONTACT_LIST |
			CM_SIGNAL_PROCESS_CURRENT_BUNDLES
		);
		hal_queue_override_to_back(cm_queue, &cm_signal);
	}
}

static bool process_signal(
	struct router_signal signal,
	QueueIdentifier_t bp_signaling_queue,
	QueueIdentifier_t router_signaling_queue,
	Semaphore_t cm_semaphore,
	QueueIdentifier_t cm_queue,
	Semaphore_t ro_sem)
{
	bool success = true;
	bundleid_t b_id;
	struct bundle *b;
	struct routed_bundle *rb;
	struct contact *contact;
	struct router_command *command;
	struct node *node;

	switch (signal.type) {
	case ROUTER_SIGNAL_PROCESS_COMMAND:
		command = (struct router_command *)signal.data;
		hal_semaphore_take_blocking(cm_semaphore);
		success = process_router_command(
			command,
			bp_signaling_queue
		);
		hal_semaphore_release(cm_semaphore);
		if (success)
			wake_up_contact_manager(
				cm_queue,
				CM_SIGNAL_UPDATE_CONTACT_LIST
			);
		if (success) {
			LOGF("RouterTask: Command (T = %c) processed.",
			     command->type);
		} else {
			LOGF("RouterTask: Processing command (T = %c) failed!",
			     command->type);
		}
		free(command);
		hal_semaphore_release(ro_sem); /* Allow optimizer to run */
		break;
	case ROUTER_SIGNAL_ROUTE_BUNDLE:
		b_id = (bundleid_t)(uintptr_t)signal.data;
		b = bundle_storage_get(b_id);
		hal_semaphore_take_blocking(cm_semaphore);
		/*
		 * TODO: Check bundle expiration time
		 * => no timely contact signal
		 */

		struct bundle_processing_result proc_result = {
			.status_or_fragments = BUNDLE_RESULT_INVALID
		};

		if (b != NULL)
			proc_result = process_bundle(b);
		b = NULL; /* b may be invalid or free'd now */
		hal_semaphore_release(cm_semaphore);
		if (IS_DEBUG_BUILD)
			LOGF(
				"RouterTask: Bundle #%d [ %s ] [ frag = %d ]",
				b_id,
				(proc_result.status_or_fragments < 1)
					? "ERR" : "OK",
				proc_result.status_or_fragments
			);
		if (proc_result.status_or_fragments < 1) {
			const enum bundle_status_report_reason reason =
				get_reason(proc_result.status_or_fragments);
			const enum bundle_processor_signal_type signal =
				get_bp_signal(proc_result.status_or_fragments);

			bundle_processor_inform(
				bp_signaling_queue,
				b_id,
				signal,
				reason
			);
			success = false;
		} else {
			for (int32_t i = 0;
			     i < proc_result.status_or_fragments; i++) {
				bundle_processor_inform(
					bp_signaling_queue,
					proc_result.fragment_ids[i],
					BP_SIGNAL_BUNDLE_ROUTED,
					BUNDLE_SR_REASON_NO_INFO
				);
			}
			wake_up_contact_manager(
				cm_queue,
				CM_SIGNAL_PROCESS_CURRENT_BUNDLES
			);
		}
		hal_semaphore_release(ro_sem); /* Allow optimizer to run */
		break;
	case ROUTER_SIGNAL_CONTACT_OVER:
		contact = (struct contact *)signal.data;
		hal_semaphore_take_blocking(cm_semaphore);
		routing_table_contact_passed(
			contact, bp_signaling_queue);
		hal_semaphore_release(cm_semaphore);
		break;
	case ROUTER_SIGNAL_TRANSMISSION_SUCCESS:
	case ROUTER_SIGNAL_TRANSMISSION_FAILURE:
		rb = (struct routed_bundle *)signal.data;
		if (rb->serialized == rb->contact_count) {
			b_id = rb->id;
			bundle_processor_inform(
				bp_signaling_queue, b_id,
				(rb->serialized == rb->transmitted)
					? BP_SIGNAL_TRANSMISSION_SUCCESS
					: BP_SIGNAL_TRANSMISSION_FAILURE,
				BUNDLE_SR_REASON_NO_INFO
			);
			free(rb->destination);
			free(rb->contacts);
			free(rb);
		}
		break;
	case ROUTER_SIGNAL_OPTIMIZATION_DROP:
		/* Should probably never occur... */
		/* If optimization failed to schedule a preempted bundle */
		rb = (struct routed_bundle *)signal.data;
		b_id = rb->id;
		bundle_processor_inform(
			bp_signaling_queue, b_id,
			BP_SIGNAL_TRANSMISSION_FAILURE,
			BUNDLE_SR_REASON_NO_INFO
		);
		free(rb->destination);
		free(rb->contacts);
		free(rb);
		LOGF("RouterTask: Preemption routing failed for bundle #%d!",
		     b_id);
		break;
	case ROUTER_SIGNAL_WITHDRAW_NODE:
		node = (struct node *)signal.data;
		hal_semaphore_take_blocking(cm_semaphore);
		routing_table_delete_node_by_eid(
			node->eid,
			router_signaling_queue
		);
		hal_semaphore_release(cm_semaphore);
		LOGF("RouterTask: Node withdrawn (%p)!", node);
		break;
	case ROUTER_SIGNAL_NEW_LINK_ESTABLISHED:
		// NOTE: When we implement a "bundle backlog", we will attempt
		// to route the bundles here.
		wake_up_contact_manager(
			cm_queue,
			CM_SIGNAL_PROCESS_CURRENT_BUNDLES
		);
		break;
	default:
		LOGF("RouterTask: Invalid signal (%d) received!", signal.type);
		success = false;
		break;
	}
	return success;
}

static bool process_router_command(
	struct router_command *router_cmd,
	QueueIdentifier_t bp_signaling_queue)
{
	/* This sorts and removes duplicates */
	if (!node_prepare_and_verify(router_cmd->data)) {
		free_node(router_cmd->data);
		return false;
	}
	switch (router_cmd->type) {
	case ROUTER_COMMAND_ADD:
		return routing_table_add_node(
			router_cmd->data,
			bp_signaling_queue
		);
	case ROUTER_COMMAND_UPDATE:
		return routing_table_replace_node(
			router_cmd->data,
			bp_signaling_queue
		);
	case ROUTER_COMMAND_DELETE:
		return routing_table_delete_node(
			router_cmd->data,
			bp_signaling_queue
		);
	default:
		free_node(router_cmd->data);
		return false;
	}
}

static struct bundle_processing_result apply_fragmentation(
	struct bundle *bundle, struct router_result route);

static struct bundle_processing_result process_bundle(struct bundle *bundle)
{
	struct router_result route;
	struct bundle_processing_result result = {
		.status_or_fragments = BUNDLE_RESULT_NO_ROUTE
	};

	ASSERT(bundle != NULL);

	if (bundle_get_expiration_time_s(bundle) < hal_time_get_timestamp_s()) {
		// Bundle is already expired on arrival at the router...
		result.status_or_fragments = BUNDLE_RESULT_EXPIRED;
		return result;
	}

	route = router_get_first_route(bundle);
	/* TODO: Add to list if no route but own OR priority > X */
	if (route.fragments == 1) {
		result.fragment_ids[0] = bundle->id;
		if (router_add_bundle_to_route(&route.fragment_results[0],
					       bundle))
			result.status_or_fragments = 1;
		else
			result.status_or_fragments = BUNDLE_RESULT_NO_MEMORY;
	} else if (!bundle_must_not_fragment(bundle)) {
		// Only fragment if it is allowed -- if not, there is no route.
		result = apply_fragmentation(bundle, route);
	}

	return result;
}

static struct bundle_processing_result apply_fragmentation(
	struct bundle *bundle, struct router_result route)
{
	struct bundle *frags[ROUTER_MAX_FRAGMENTS];
	uint32_t size;
	int32_t f, g;
	int32_t fragments = route.fragments;
	struct bundle_processing_result result = {
		.status_or_fragments = BUNDLE_RESULT_NO_MEMORY
	};

	/* Create fragments */
	frags[0] = bundlefragmenter_initialize_first_fragment(bundle);
	if (frags[0] == NULL)
		return result;

	for (f = 0; f < fragments - 1; f++) {
		/* Determine minimal fragmented bundle size */
		if (f == 0)
			size = bundle_get_first_fragment_min_size(bundle);
		else if (f == fragments - 1)
			size = bundle_get_last_fragment_min_size(bundle);
		else
			size = bundle_get_mid_fragment_min_size(bundle);

		frags[f + 1] = bundlefragmenter_fragment_bundle(frags[f],
			size + route.fragment_results[f].payload_size);

		if (frags[f + 1] == NULL) {
			for (g = 0; g <= f; g++)
				bundle_free(frags[g]);
			return result;
		}
	}

	/* Add to route */
	for (f = 0; f < fragments; f++) {
		bundle_storage_add(frags[f]);
		if (!router_add_bundle_to_route(
			&route.fragment_results[f], frags[f])
		) {
			for (g = 0; g < f; g++)
				router_remove_bundle_from_route(
					&route.fragment_results[g],
					frags[g]->id, 1);
			for (g = 0; g < fragments; g++) {
				/* FIXME: Routed bundles not unrouted */
				if (g <= f)
					bundle_storage_delete(frags[g]->id);
				bundle_free(frags[g]);
			}
			return result;
		}
	}

	/* Success - remove bundle */
	bundle_storage_delete(bundle->id);
	bundle_free(bundle);

	for (f = 0; f < fragments; f++)
		result.fragment_ids[f] = frags[f]->id;
	result.status_or_fragments = fragments;
	return result;
}
