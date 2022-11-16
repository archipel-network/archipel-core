// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/bundle.h"
#include "ud3tn/bundle_fragmenter.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/contact_manager.h"
#include "ud3tn/node.h"
#include "ud3tn/router.h"
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
	QueueIdentifier_t cm_queue);

static bool process_router_command(
	struct router_command *router_cmd,
	QueueIdentifier_t bp_signaling_queue);

struct bundle_processing_result {
	int32_t status_or_fragments;
	struct bundle *fragments[ROUTER_MAX_FRAGMENTS];
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

	for (;;) {
		if (hal_queue_receive(
			parameters->router_signaling_queue, &signal,
			-1) == UD3TN_OK
		) {
			process_signal(signal,
				parameters->bundle_processor_signaling_queue,
				parameters->router_signaling_queue,
				cm_param.semaphore, cm_param.control_queue);
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
	QueueIdentifier_t cm_queue)
{
	bool success = true;
	struct bundle *b;
	struct contact *contact;
	struct router_command *command;
	struct node *node;
	struct bundle_tx_result *tx_result;

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
		break;
	case ROUTER_SIGNAL_ROUTE_BUNDLE:
		b = (struct bundle *)signal.data;

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

		// b should not be used anymore, may be dropped due to
		// fragmentation.
		struct bundle *const b_old_ptr = b;

		b = NULL;

		hal_semaphore_release(cm_semaphore);
		if (IS_DEBUG_BUILD)
			LOGF(
				"RouterTask: Bundle %p [ %s ] [ frag = %d ]",
				b_old_ptr,
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
				b_old_ptr,  // safe to use -- not fragmented
				signal,
				reason
			);
			success = false;
		} else {
			/* 5.4-4 */
			/* We do not accept custody -> only inform CM */
			wake_up_contact_manager(
				cm_queue,
				CM_SIGNAL_PROCESS_CURRENT_BUNDLES
			);
		}
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
		tx_result = (struct bundle_tx_result *)signal.data;
		b = tx_result->bundle;
		free(tx_result->peer_cla_addr);
		free(tx_result);
		bundle_processor_inform(
			bp_signaling_queue, b,
			(signal.type == ROUTER_SIGNAL_TRANSMISSION_SUCCESS)
				? BP_SIGNAL_TRANSMISSION_SUCCESS
				: BP_SIGNAL_TRANSMISSION_FAILURE,
			BUNDLE_SR_REASON_NO_INFO
		);
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
		// XXX: We do not use the provided CLA address.
		free(signal.data);
		// NOTE: When we implement a "bundle backlog", we will attempt
		// to route the bundles here.
		wake_up_contact_manager(
			cm_queue,
			CM_SIGNAL_PROCESS_CURRENT_BUNDLES
		);
		break;
	case ROUTER_SIGNAL_LINK_DOWN:
		// XXX: We do not use the provided CLA address.
		free(signal.data);
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
	uint64_t timestamp_s = hal_time_get_timestamp_s();

	if (bundle_get_expiration_time_s(bundle, timestamp_s) < timestamp_s) {
		// Bundle is already expired on arrival at the router...
		result.status_or_fragments = BUNDLE_RESULT_EXPIRED;
		return result;
	}

	route = router_get_first_route(bundle);
	if (route.fragments == 1) {
		result.fragments[0] = bundle;
		if (router_add_bundle_to_contact(
				route.fragment_results[0].contact,
				bundle) == UD3TN_OK)
			result.status_or_fragments = 1;
		else
			result.status_or_fragments = BUNDLE_RESULT_NO_MEMORY;
	} else if (route.fragments && !bundle_must_not_fragment(bundle)) {
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
		} else if (frags[f] == frags[f + 1]) {
			// Not fragmented b/c not needed - the router does some
			// conservative estimations regarding size of CBOR ints
			// that may lead to fewer actual fragments here.
			// Just update the count accordingly and do not schedule
			// the rest.
			route.fragments = fragments = f + 1;
			frags[f + 1] = NULL;
			break;
		}
	}

	/* Add to route */
	for (f = 0; f < fragments; f++) {
		if (router_add_bundle_to_contact(
				route.fragment_results[f].contact,
				frags[f]) != UD3TN_OK) {
			LOGF("RouterTask: Scheduling bundle %p failed, dropping all fragments.",
			     bundle);
			// Remove from all previously-scheduled routes
			for (g = 0; g < f; g++)
				router_remove_bundle_from_contact(
					route.fragment_results[g].contact,
					frags[g]
				);
			// Drop _all_ fragments
			for (g = 0; g < fragments; g++)
				bundle_free(frags[g]);
			return result;
		}
	}

	/* Success - remove bundle */
	bundle_free(bundle);

	for (f = 0; f < fragments; f++)
		result.fragments[f] = frags[f];
	result.status_or_fragments = fragments;
	return result;
}
