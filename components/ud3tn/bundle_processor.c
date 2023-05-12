// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/agent_manager.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/contact_manager.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/eid.h"
#include "ud3tn/report_manager.h"
#include "ud3tn/result.h"
#include "ud3tn/router.h"

#include "agents/config_agent.h"

#include "bundle6/bundle6.h"
#include "bundle7/bundle_age.h"
#include "bundle7/hopcount.h"

#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"

#include "cbor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum bundle_handling_result {
	BUNDLE_HRESULT_OK = 0,
	BUNDLE_HRESULT_DELETED,
	BUNDLE_HRESULT_BLOCK_DISCARDED,
};

struct bp_context {
	QueueIdentifier_t out_queue;
	const char *local_eid;
	char *local_eid_prefix;
	bool local_eid_is_ipn;
	bool status_reporting;

	struct contact_manager_params cm_param;

	struct reassembly_list {
		struct reassembly_bundle_list {
			struct bundle *bundle;
			struct reassembly_bundle_list *next;
		} *bundle_list;
		struct reassembly_list *next;
	} *reassembly_list;

	struct known_bundle_list {
		struct bundle_unique_identifier id;
		uint64_t deadline_ms;
		struct known_bundle_list *next;
	} *known_bundle_list;
};

/* DECLARATIONS */

static inline void handle_signal(
	struct bp_context *const ctx,
	const struct bundle_processor_signal signal);

static void handle_contact_over(
	const struct bp_context *const ctx, struct contact *contact);

static enum ud3tn_result bundle_dispatch(
	struct bp_context *const ctx, struct bundle *bundle);
static bool bundle_endpoint_is_local(
	const struct bp_context *const ctx, struct bundle *bundle);
static enum ud3tn_result bundle_forward(
	const struct bp_context *const ctx, struct bundle *bundle);
static void bundle_forwarding_success(
	const struct bp_context *const ctx, struct bundle *bundle);
static void bundle_forwarding_contraindicated(
	const struct bp_context *const ctx,
	struct bundle *bundle, enum bundle_status_report_reason reason);
static void bundle_forwarding_failed(
	const struct bp_context *const ctx,
	struct bundle *bundle, enum bundle_status_report_reason reason);
static void bundle_expired(
	const struct bp_context *const ctx, struct bundle *bundle);
static void bundle_receive(
	struct bp_context *const ctx, struct bundle *bundle);
static enum bundle_handling_result handle_unknown_block_flags(
	const struct bp_context *const ctx,
	struct bundle *bundle, enum bundle_block_flags flags);
static void bundle_deliver_local(
	struct bp_context *const ctx, struct bundle *bundle);
static void bundle_attempt_reassembly(
	struct bp_context *const ctx, struct bundle *bundle);
static void bundle_deliver_adu(
	const struct bp_context *const ctx, struct bundle_adu data);
static void bundle_delete(
	const struct bp_context *const ctx,
	struct bundle *bundle, enum bundle_status_report_reason reason);
static void bundle_discard(struct bundle *bundle);
static void bundle_handle_custody_signal(
	struct bundle_administrative_record *signal);
static void bundle_dangling(
	const struct bp_context *const ctx, struct bundle *bundle);
static bool hop_count_validation(struct bundle *bundle);
static const char *get_agent_id(
	const struct bp_context *const ctx, const char *dest_eid);
static bool bundle_record_add_and_check_known(
	struct bp_context *const ctx, const struct bundle *bundle);
static bool bundle_reassembled_is_known(
	struct bp_context *const ctx, const struct bundle *bundle);
static void bundle_add_reassembled_as_known(
	struct bp_context *const ctx, const struct bundle *bundle);

static void send_status_report(
	const struct bp_context *const ctx,
	struct bundle *bundle,
	const enum bundle_status_report_status_flags status,
	const enum bundle_status_report_reason reason);
static enum ud3tn_result send_bundle(
	const struct bp_context *const ctx, struct bundle *bundle);

static inline void bundle_add_rc(struct bundle *bundle,
	const enum bundle_retention_constraints constraint)
{
	bundle->ret_constraints |= constraint;
}

static inline void bundle_rem_rc(struct bundle *bundle,
	const enum bundle_retention_constraints constraint, int discard)
{
	bundle->ret_constraints &= ~constraint;
	if (discard && bundle->ret_constraints == BUNDLE_RET_CONSTRAINT_NONE)
		bundle_discard(bundle);
}

static void wake_up_contact_manager(QueueIdentifier_t cm_queue,
				    enum contact_manager_signal cm_signal);
static void bundle_resched_func(struct bundle *bundle, const void *ctx);

/* COMMUNICATION */

void bundle_processor_inform(
	QueueIdentifier_t bundle_processor_signaling_queue,
	struct bundle *bundle,
	enum bundle_processor_signal_type type,
	char *peer_cla_addr,
	struct agent_manager_parameters *agent_manager_params,
	struct contact *contact,
	struct router_command *router_cmd)
{
	struct bundle_processor_signal signal = {
		.type = type,
		.bundle = bundle,
		.peer_cla_addr = peer_cla_addr,
		.agent_manager_params = agent_manager_params,
		.contact = contact,
		.router_cmd = router_cmd,
	};

	hal_queue_push_to_back(bundle_processor_signaling_queue, &signal);
}

int bundle_processor_perform_agent_action(
	QueueIdentifier_t signaling_queue,
	enum bundle_processor_signal_type type,
	const char *sink_identifier,
	void (*const callback)(struct bundle_adu data, void *param,
			       const void *bp_context),
	void *param,
	bool wait_for_feedback)
{
	struct agent_manager_parameters *aaps;
	QueueIdentifier_t feedback_queue;
	int result;

	ASSERT((type == BP_SIGNAL_AGENT_REGISTER && callback)
		|| type == BP_SIGNAL_AGENT_DEREGISTER);
	ASSERT(sink_identifier);

	aaps = malloc(sizeof(struct agent_manager_parameters));
	if (!aaps)
		return -1;

	aaps->agent.sink_identifier = sink_identifier;
	aaps->agent.callback = callback;
	aaps->agent.param = param;
	aaps->feedback_queue = NULL;

	if (!wait_for_feedback) {
		bundle_processor_inform(
			signaling_queue,
			NULL,
			type,
			NULL,
			aaps,
			NULL,
			NULL
		);
		return 0;
	}

	feedback_queue = hal_queue_create(1, sizeof(int));
	if (!feedback_queue) {
		free(aaps);
		return -1;
	}

	aaps->feedback_queue = feedback_queue;
	bundle_processor_inform(
		signaling_queue,
		NULL,
		type,
		NULL,
		aaps,
		NULL,
		NULL
	);

	if (hal_queue_receive(feedback_queue, &result, -1) == UD3TN_OK) {
		hal_queue_delete(feedback_queue);
		return result;
	}

	hal_queue_delete(feedback_queue);
	return -1;
}

void bundle_processor_handle_router_command(
	void *const bp_context, struct router_command *cmd)
{
	const struct bp_context *const ctx = bp_context;

	hal_semaphore_take_blocking(ctx->cm_param.semaphore);

	// NOTE: May invoke router via bundle_dangling!
	enum ud3tn_result result = router_process_command(
		cmd,
		(struct rescheduling_handle) {
			.reschedule_func = bundle_resched_func,
			.reschedule_func_context = ctx,
		}
	);

	hal_semaphore_release(ctx->cm_param.semaphore);

	if (result == UD3TN_OK) {
		wake_up_contact_manager(
			ctx->cm_param.control_queue,
			CM_SIGNAL_UPDATE_CONTACT_LIST
		);
	}
}

void bundle_processor_task(void * const param)
{
	struct bundle_processor_task_parameters *p =
		(struct bundle_processor_task_parameters *)param;
	struct bundle_processor_signal signal;
	struct bp_context ctx = {
		.out_queue = NULL,
		.local_eid = p->local_eid,
		.local_eid_prefix = NULL,
		.status_reporting = p->status_reporting,
		.reassembly_list = NULL,
		.known_bundle_list = NULL,
	};

	ASSERT(strlen(ctx.local_eid) > 3);
	if (get_eid_scheme(ctx.local_eid) == EID_SCHEME_IPN) {
		ctx.local_eid_is_ipn = true;
		ctx.local_eid_prefix = strdup(ctx.local_eid);

		char *const dot = strchr(ctx.local_eid_prefix, '.');

		if (!dot) {
			LOGF("BundleProcessor: Invalid local EID \"%s\"",
			     ctx.local_eid_prefix);
			ASSERT(false);
		} else {
			dot[1] = '\0'; // truncate string after dot
		}
	} else {
		ctx.local_eid_prefix = strdup(ctx.local_eid);

		const size_t len = strlen(ctx.local_eid_prefix);

		// remove slash if it is there to also match EIDs without
		if (ctx.local_eid_prefix[len - 1] == '/')
			ctx.local_eid_prefix[len - 1] = '\0';
	}

	/* Init routing tables */
	ASSERT(routing_table_init() == UD3TN_OK);
	/* Start contact manager */
	ctx.cm_param = contact_manager_start(
		p->signaling_queue,
		routing_table_get_raw_contact_list_ptr());
	if (ctx.cm_param.task_creation_result != UD3TN_OK) {
		LOG("BundleProcessor: Contact manager could not be initialized!");
		ASSERT(false);
	}

	if (config_agent_setup(p->signaling_queue, ctx.local_eid,
			       p->allow_remote_configuration, &ctx)) {
		LOG("BundleProcessor: Config agent could not be initialized!");
		ASSERT(false);
	}

	LOGF("BundleProcessor: BPA initialized for \"%s\", status reports %s",
	     p->local_eid, p->status_reporting ? "enabled" : "disabled");

	for (;;) {
		if (hal_queue_receive(p->signaling_queue, &signal,
			-1) == UD3TN_OK
		) {
			handle_signal(&ctx, signal);
		}
	}
}

static inline void handle_signal(
	struct bp_context *const ctx,
	const struct bundle_processor_signal signal)
{
	struct agent_manager_parameters *aaps;
	int feedback;

	switch (signal.type) {
	case BP_SIGNAL_BUNDLE_INCOMING:
		bundle_receive(ctx, signal.bundle);
		break;
	case BP_SIGNAL_TRANSMISSION_SUCCESS:
		bundle_forwarding_success(ctx, signal.bundle);
		// XXX: We do not use the provided CLA address.
		free(signal.peer_cla_addr);
		break;
	case BP_SIGNAL_TRANSMISSION_FAILURE:
		bundle_forwarding_failed(
			ctx,
			signal.bundle,
			BUNDLE_SR_REASON_TRANSMISSION_CANCELED
		);
		// XXX: We do not use the provided CLA address.
		free(signal.peer_cla_addr);
		break;
	case BP_SIGNAL_BUNDLE_LOCAL_DISPATCH:
		bundle_dispatch(ctx, signal.bundle);
		break;
	case BP_SIGNAL_AGENT_REGISTER:
		aaps = signal.agent_manager_params;
		feedback = agent_register(aaps->agent.sink_identifier,
			aaps->agent.callback, aaps->agent.param);
		if (aaps->feedback_queue)
			hal_queue_push_to_back(aaps->feedback_queue, &feedback);
		free(aaps);
		break;
	case BP_SIGNAL_AGENT_DEREGISTER:
		aaps = signal.agent_manager_params;
		feedback = agent_deregister(aaps->agent.sink_identifier);
		if (aaps->feedback_queue)
			hal_queue_push_to_back(aaps->feedback_queue, &feedback);
		free(aaps);
		break;
	case BP_SIGNAL_NEW_LINK_ESTABLISHED:
		// XXX: We do not use the provided CLA address.
		free(signal.peer_cla_addr);
		// NOTE: When we implement a "bundle backlog", we will attempt
		// to route the bundles here.
		wake_up_contact_manager(
			ctx->cm_param.control_queue,
			CM_SIGNAL_PROCESS_CURRENT_BUNDLES
		);
		break;
	case BP_SIGNAL_LINK_DOWN:
		// XXX: We do not use the provided CLA address.
		free(signal.peer_cla_addr);
		break;
	case BP_SIGNAL_CONTACT_OVER:
		handle_contact_over(ctx, signal.contact);
		break;
	default:
		LOGF("BundleProcessor: Invalid signal (%d) detected",
		     signal.type);
		break;
	}
}

static void handle_contact_over(
	const struct bp_context *const ctx, struct contact *contact)
{
	hal_semaphore_take_blocking(ctx->cm_param.semaphore);
	// NOTE: May invoke router via bundle_dangling!
	routing_table_contact_passed(
		contact,
		(struct rescheduling_handle) {
			.reschedule_func = bundle_resched_func,
			.reschedule_func_context = ctx,
		}
	);
	hal_semaphore_release(ctx->cm_param.semaphore);
}

/* BUNDLE HANDLING */

/* 5.3 */
static enum ud3tn_result bundle_dispatch(
	struct bp_context *const ctx, struct bundle *bundle)
{
	LOGF("BundleProcessor: Dispatching bundle %p (from = %s, to = %s)",
	     bundle, bundle->source, bundle->destination);
	/* 5.3-1 */
	if (bundle_endpoint_is_local(ctx, bundle)) {
		bundle_deliver_local(ctx, bundle);
		return UD3TN_OK;
	}
	/* 5.3-2 */
	return bundle_forward(ctx, bundle);
}

enum ud3tn_result bundle_processor_bundle_dispatch(
	void *bp_context, struct bundle *bundle)
{
	return bundle_dispatch(bp_context, bundle);
}

static bool endpoint_is_local(
	const struct bp_context *const ctx, const char *eid)
{
	const size_t local_len = strlen(ctx->local_eid_prefix);
	const size_t dest_len = strlen(eid);

	/* Compare EID _prefix_ with configured uD3TN EID */
	return (
		// For the memcmp to be safe, the tested EID has to be at
		// least as long as the local EID.
		dest_len >= local_len &&
		// The prefix (the local EID) has to match the EID.
		memcmp(ctx->local_eid_prefix, eid,
		       local_len) == 0
	);
}

/* 5.3-1 */
static bool bundle_endpoint_is_local(
	const struct bp_context *const ctx, struct bundle *bundle)
{
	return endpoint_is_local(ctx, bundle->destination);
}

/* 5.4 */
static enum ud3tn_result bundle_forward(
	const struct bp_context *const ctx, struct bundle *bundle)
{
	/* 4.3.4. Hop Count (BPv7-bis) */
	if (!hop_count_validation(bundle)) {
		LOGF("BundleProcessor: Deleting bundle %p: Hop Limit Exceeded", bundle);
		bundle_delete(ctx, bundle, BUNDLE_SR_REASON_HOP_LIMIT_EXCEEDED);
		return UD3TN_FAIL;
	}

	/* 5.4-1 */
	bundle_add_rc(bundle, BUNDLE_RET_CONSTRAINT_FORWARD_PENDING);
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING, 0);
	/* 5.4-2 */
	if (send_bundle(ctx, bundle) != UD3TN_OK)
		return UD3TN_FAIL;
	/* For steps after 5.4-2, see below */
	return UD3TN_OK;
}

/* 5.4-6 */
static void bundle_forwarding_success(
	const struct bp_context *const ctx, struct bundle *bundle)
{
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_REPORT_FORWARDING)) {
		/* See 5.4-6: reason code vs. unidirectional links */
		send_status_report(
			ctx,
			bundle,
			BUNDLE_SR_FLAG_BUNDLE_FORWARDED,
			BUNDLE_SR_REASON_NO_INFO
		);
	}
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_FORWARD_PENDING, 0);
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_FLAG_OWN, 1);
}

/* 5.4.1 */
static void bundle_forwarding_contraindicated(
	const struct bp_context *const ctx,
	struct bundle *bundle, enum bundle_status_report_reason reason)
{
	/* 5.4.1-1: For now, we declare forwarding failure everytime */
	bundle_forwarding_failed(ctx, bundle, reason);
	/* 5.4.1-2 (a): At the moment, custody transfer is declared as failed */
	/* 5.4.1-2 (b): Will not be handled */
}

/* 5.4.2 */
static void bundle_forwarding_failed(
	const struct bp_context *const ctx,
	struct bundle *bundle, enum bundle_status_report_reason reason)
{
	LOGF("BundleProcessor: Deleting bundle %p: Forwarding Failed",
	     bundle);
	bundle_delete(ctx, bundle, reason);
}

/* 5.5 */
static void bundle_expired(
	const struct bp_context *const ctx, struct bundle *bundle)
{
	LOGF("BundleProcessor: Deleting bundle %p: Lifetime Expired",
	     bundle);
	bundle_delete(ctx, bundle, BUNDLE_SR_REASON_LIFETIME_EXPIRED);
}

/* 5.6 */
static void bundle_receive(struct bp_context *const ctx, struct bundle *bundle)
{
	struct bundle_block_list **e;
	enum bundle_handling_result res;

	// Set the reception time to calculate the bundle's residence time
	bundle->reception_timestamp_ms = hal_time_get_timestamp_ms();

	/* 5.6-1 Add retention constraint */
	bundle_add_rc(bundle, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING);
	/* 5.6-2 Request reception */
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_REPORT_RECEPTION))
		send_status_report(
			ctx,
			bundle,
			BUNDLE_SR_FLAG_BUNDLE_RECEIVED,
			BUNDLE_SR_REASON_NO_INFO
		);

	// Check lifetime
	const uint64_t timestamp_ms = hal_time_get_timestamp_ms();

	if (bundle_get_expiration_time_ms(bundle) < timestamp_ms) {
		bundle_expired(ctx, bundle);
		return;
	}

	/* 5.6-3 Handle blocks */
	e = &bundle->blocks;
	while (*e != NULL) {
		if ((*e)->data->type != BUNDLE_BLOCK_TYPE_PAYLOAD) {
			res = handle_unknown_block_flags(
				ctx,
				bundle,
				(*e)->data->flags
			);
			switch (res) {
			case BUNDLE_HRESULT_OK:
				(*e)->data->flags |=
					BUNDLE_V6_BLOCK_FLAG_FWD_UNPROC;
				break;
			case BUNDLE_HRESULT_DELETED:
				LOGF("BundleProcessor: Deleting bundle %p: Block Unintelligible",
				     bundle);
				bundle_delete(
					ctx,
					bundle,
					BUNDLE_SR_REASON_BLOCK_UNINTELLIGIBLE
				);
				return;
			case BUNDLE_HRESULT_BLOCK_DISCARDED:
				*e = bundle_block_entry_free(*e);
				break;
			}

		}
		if (*e != NULL)
			e = &(*e)->next;
	}

	/* NOTE: Test for custody acceptance here. */
	/* NOTE: We never accept custody, we do not have persistent storage. */

	/* 5.6-5 */
	bundle_dispatch(ctx, bundle);
}

/* 5.6-3 */
static enum bundle_handling_result handle_unknown_block_flags(
	const struct bp_context *const ctx,
	struct bundle *bundle, enum bundle_block_flags flags)
{
	if (HAS_FLAG(flags, BUNDLE_BLOCK_FLAG_REPORT_IF_UNPROC)) {
		send_status_report(
			ctx,
			bundle,
			BUNDLE_SR_FLAG_BUNDLE_RECEIVED,
			BUNDLE_SR_REASON_BLOCK_UNINTELLIGIBLE
		);
	}
	if (HAS_FLAG(flags, BUNDLE_BLOCK_FLAG_DELETE_BUNDLE_IF_UNPROC))
		return BUNDLE_HRESULT_DELETED;
	else if (HAS_FLAG(flags, BUNDLE_BLOCK_FLAG_DISCARD_IF_UNPROC))
		return BUNDLE_HRESULT_BLOCK_DISCARDED;
	return BUNDLE_HRESULT_OK;
}

/* 5.7 */
static void bundle_deliver_local(
	struct bp_context *const ctx, struct bundle *bundle)
{
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING, 0);

	/* Check and record knowledge of bundle */
	if (bundle_record_add_and_check_known(ctx, bundle)) {
		LOGF("BundleProcessor: Bundle %p was already delivered, dropping.",
		     bundle);
		// NOTE: We cannot have custody as the CM checks for duplicates
		bundle_discard(bundle);
		return;
	}

	/* Report successful delivery, if applicable */
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_REPORT_DELIVERY)) {
		send_status_report(
			ctx,
			bundle,
			BUNDLE_SR_FLAG_BUNDLE_DELIVERED,
			BUNDLE_SR_REASON_NO_INFO
		);
	}

	if (!HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_ADMINISTRATIVE_RECORD) &&
			get_agent_id(ctx, bundle->destination) == NULL) {
		// If it is no admin. record and we have no agent to deliver
		// it to, drop it.
		LOGF("BundleProcessor: Received bundle not destined for any registered EID (from = %s, to = %s), dropping.",
		     bundle->source, bundle->destination);
		bundle_delete(
			ctx,
			bundle,
			BUNDLE_SR_REASON_DEST_EID_UNINTELLIGIBLE
		);
		return;
	}

	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_IS_FRAGMENT)) {
		bundle_add_rc(bundle, BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING);
		bundle_attempt_reassembly(ctx, bundle);
	} else {
		struct bundle_adu adu = bundle_to_adu(bundle);

		bundle_discard(bundle);
		bundle_deliver_adu(ctx, adu);
	}
}

static bool may_reassemble(const struct bundle *b1, const struct bundle *b2)
{
	return (
		b1->creation_timestamp_ms == b2->creation_timestamp_ms &&
		b1->sequence_number == b2->sequence_number &&
		strcmp(b1->source, b2->source) == 0 // XXX: '==' may be ok
	);
}

static void add_to_reassembly_bundle_list(
	const struct bp_context *const ctx,
	struct reassembly_list *item,
	struct bundle *bundle)
{
	struct reassembly_bundle_list **cur_entry = &item->bundle_list;

	while (*cur_entry != NULL) {
		struct reassembly_bundle_list *e = *cur_entry;

		// Order by frag. offset
		if (e->bundle->fragment_offset > bundle->fragment_offset)
			break;
		cur_entry = &(*cur_entry)->next;
	}

	struct reassembly_bundle_list *new_entry = malloc(
		sizeof(struct reassembly_bundle_list)
	);
	if (!new_entry) {
		LOGF("BundleProcessor: Deleting bundle %p: Cannot store in reassembly list.",
		     bundle);
		bundle_delete(ctx, bundle, BUNDLE_SR_REASON_DEPLETED_STORAGE);
		return;
	}
	new_entry->bundle = bundle;
	new_entry->next = *cur_entry;
	*cur_entry = new_entry;
}

static void try_reassemble(
	struct bp_context *const ctx, struct reassembly_list **slot)
{
	struct reassembly_list *const e = *slot;
	struct reassembly_bundle_list *eb;
	struct bundle *b;

	size_t pos_in_bundle = 0;

	LOG("BundleProcessor: Attempting bundle reassembly!");

	// Check if we can reassemble
	for (eb = e->bundle_list; eb; eb = eb->next) {
		b = eb->bundle;
		if (b->fragment_offset > pos_in_bundle)
			return; // cannot reassemble, has gaps
		pos_in_bundle = b->fragment_offset + b->payload_block->length;
		if (pos_in_bundle >= b->total_adu_length)
			break; // can reassemble
	}
	if (!eb)
		return;
	LOG("BundleProcessor: Reassembling bundle!");

	// Reassemble by memcpy
	b = e->bundle_list->bundle;
	const size_t adu_length = b->total_adu_length;
	uint8_t *const payload = malloc(adu_length);
	bool added_as_known = false;

	if (!payload)
		return; // currently not enough memory to reassemble

	struct bundle_adu adu = bundle_adu_init(b);

	adu.payload = payload;
	adu.length = adu_length;

	pos_in_bundle = 0;
	for (eb = e->bundle_list; eb; eb = eb->next) {
		b = eb->bundle;

		if (!added_as_known) {
			bundle_add_reassembled_as_known(ctx, b);
			added_as_known = true;
		}

		const size_t offset_in_bundle = (
			pos_in_bundle - b->fragment_offset
		);
		const size_t bytes_copied = MIN(
			b->payload_block->length - offset_in_bundle,
			adu_length - pos_in_bundle
		);

		if (offset_in_bundle < b->payload_block->length) {
			memcpy(
				&payload[pos_in_bundle],
				&b->payload_block->data[offset_in_bundle],
				bytes_copied
			);
			pos_in_bundle += bytes_copied;
		}

		bundle_rem_rc(b, BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING, 0);
		bundle_discard(b);
	}

	// Delete slot
	*slot = (*slot)->next;
	while (e->bundle_list) {
		eb = e->bundle_list;
		e->bundle_list = e->bundle_list->next;
		free(eb);
	}
	free(e);

	// Deliver ADU
	bundle_deliver_adu(ctx, adu);
}

static void bundle_attempt_reassembly(
	struct bp_context *const ctx, struct bundle *bundle)
{
	struct reassembly_list **r_list_e = &ctx->reassembly_list;

	if (bundle_reassembled_is_known(ctx, bundle)) {
		LOGF("BundleProcessor: Original bundle for %p was already delivered, dropping.",
		     bundle);
		// Already delivered the original bundle
		bundle_rem_rc(
			bundle,
			BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING,
			0
		);
		bundle_discard(bundle);
	}

	// Find bundle
	for (; *r_list_e; r_list_e = &(*r_list_e)->next) {
		struct reassembly_list *const e = *r_list_e;

		if (may_reassemble(e->bundle_list->bundle, bundle)) {
			add_to_reassembly_bundle_list(ctx, e, bundle);
			try_reassemble(ctx, r_list_e);
			return;
		}
	}

	// Not found, append
	struct reassembly_list *new_list = malloc(
		sizeof(struct reassembly_list)
	);

	if (!new_list) {
		LOGF("BundleProcessor: Deleting bundle %p: Cannot create reassembly list.",
		     bundle);
		bundle_delete(ctx, bundle, BUNDLE_SR_REASON_DEPLETED_STORAGE);
		return;
	}
	new_list->bundle_list = NULL;
	new_list->next = NULL;
	add_to_reassembly_bundle_list(ctx, new_list, bundle);
	*r_list_e = new_list;
	try_reassemble(ctx, r_list_e);
}

static void bundle_deliver_adu(const struct bp_context *const ctx, struct bundle_adu adu)
{
	struct bundle_administrative_record *record;

	if (HAS_FLAG(adu.proc_flags, BUNDLE_FLAG_ADMINISTRATIVE_RECORD)) {
		record = parse_administrative_record(
			adu.protocol_version,
			adu.payload,
			adu.length
		);

		if (record != NULL && record->type == BUNDLE_AR_CUSTODY_SIGNAL) {
			LOGF("BundleProcessor: Received administrative record of type %u", record->type);
			bundle_handle_custody_signal(record);
			bundle_adu_free_members(adu);
		} else if (record != NULL &&
				  (record->type == BUNDLE_AR_BPDU || record->type == BUNDLE_AR_BPDU_COMPAT)) {
			ASSERT(record->start_of_record_ptr != NULL);
			ASSERT(record->start_of_record_ptr <
			       adu.payload + adu.length);
			const size_t bytes_to_skip = (
				record->start_of_record_ptr -
				adu.payload
			);

			// Remove the record-specific bytes from the ADU so
			// only the BPDU remains.
			adu.length = adu.length - bytes_to_skip;
			memmove(
				adu.payload,
				adu.payload + bytes_to_skip,
				adu.length
			);
			adu.proc_flags = BUNDLE_FLAG_ADMINISTRATIVE_RECORD;

			const char *agent_id = get_eid_scheme(ctx->local_eid) == EID_SCHEME_DTN ? "bibe" : "2925";

			ASSERT(agent_id != NULL);
			LOGF("BundleProcessor: Received BIBE bundle -> \"%s\"; len(PL) = %d B",
			     agent_id, adu.length);
			agent_forward(agent_id, adu, ctx);
		} else if (record != NULL) {
			LOGF("BundleProcessor: Received administrative record of unknown type %u, discarding.",
			     record->type);
			bundle_adu_free_members(adu);
		} else {
			LOG("BundleProcessor: Received administrative record we cannot parse, discarding.");
			bundle_adu_free_members(adu);
		}

		free_administrative_record(record);
		return;
	}

	const char *agent_id = get_agent_id(ctx, adu.destination);

	ASSERT(agent_id != NULL);
	LOGF("BundleProcessor: Received local bundle -> \"%s\"; len(PL) = %d B",
	     agent_id, adu.length);
	agent_forward(agent_id, adu, ctx);
}

/* 5.13 (BPv7) */
/* NOTE: Custody Transfer Deferral would be implemented here. */

/* 5.13 (RFC 5050) */
/* 5.14 (BPv7-bis) */
static void bundle_delete(
	const struct bp_context *const ctx,
	struct bundle *bundle, enum bundle_status_report_reason reason)
{
	bool generate_report = false;

	/* NOTE: If custody was accepted, test this here and report. */

	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_REPORT_DELETION))
		generate_report = true;

	if (generate_report)
		send_status_report(
			ctx,
			bundle,
			BUNDLE_SR_FLAG_BUNDLE_DELETED,
			reason
		);

	bundle->ret_constraints &= BUNDLE_RET_CONSTRAINT_NONE;
	bundle_discard(bundle);
}

/* 5.14 (RFC 5050) */
/* 5.15 (BPv7-bis) */
static void bundle_discard(struct bundle *bundle)
{
	bundle_drop(bundle);
}

/* 6.3 */
static void bundle_handle_custody_signal(
	struct bundle_administrative_record *signal)
{
	/* NOTE: We never accept custody, we do not have persistent storage. */
	(void)signal;
}

/* RE-SCHEDULING */
static void bundle_dangling(
	const struct bp_context *const ctx, struct bundle *bundle)
{
	uint8_t resched = (FAILED_FORWARD_POLICY == POLICY_TRY_RE_SCHEDULE);

	if (!resched) {
		LOGF("BundleProcessor: Deleting bundle %p: Forwarding failed and policy indicates to drop it.",
		     bundle);
		bundle_delete(
			ctx,
			bundle,
			BUNDLE_SR_REASON_TRANSMISSION_CANCELED
		);
	/* Send it to the router task again after evaluating policy. */
	} else {
		send_bundle(ctx, bundle);
	}
}

/* HELPERS */

static void send_status_report(
	const struct bp_context *const ctx,
	struct bundle *bundle,
	const enum bundle_status_report_status_flags status,
	const enum bundle_status_report_reason reason)
{
	if (!ctx->status_reporting)
		return;

	/* If the report-to EID is the null endpoint or uD3TN itself we do */
	/* not need to create a status report */
	if (bundle->report_to == NULL ||
	    strcmp(bundle->report_to, "dtn:none") == 0 ||
	    endpoint_is_local(ctx, bundle->report_to))
		return;

	struct bundle_status_report report = {
		.status = status,
		.reason = reason
	};
	struct bundle *b = generate_status_report(
		bundle,
		&report,
		ctx->local_eid
	);

	if (b != NULL) {
		bundle_add_rc(b, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING);
		if (bundle_forward(ctx, b) != UD3TN_OK)
			LOGF("BundleProcessor: Failed sending status report for bundle %p.",
			     bundle);
	}
}

static inline enum bundle_status_report_reason get_fail_reason(
	enum router_result_status result)
{
	switch (result) {
	case ROUTER_RESULT_NO_ROUTE:
		return BUNDLE_SR_REASON_NO_KNOWN_ROUTE;
	case ROUTER_RESULT_NO_MEMORY:
		return BUNDLE_SR_REASON_DEPLETED_STORAGE;
	case ROUTER_RESULT_EXPIRED:
		return BUNDLE_SR_REASON_LIFETIME_EXPIRED;
	case ROUTER_RESULT_NO_TIMELY_CONTACTS:
	default:
		return BUNDLE_SR_REASON_NO_TIMELY_CONTACT;
	}
}

static inline const char *get_router_status_str(
	enum router_result_status result)
{
	switch (result) {
	case ROUTER_RESULT_OK:
		return "Success";
	case ROUTER_RESULT_NO_ROUTE:
		return "No Route Found";
	case ROUTER_RESULT_NO_MEMORY:
		return "No Memory";
	case ROUTER_RESULT_EXPIRED:
		return "Expired";
	case ROUTER_RESULT_NO_TIMELY_CONTACTS:
	default:
		return "No Timely Contact";
	}
}

static enum ud3tn_result send_bundle(
	const struct bp_context *const ctx, struct bundle *bundle)
{
	hal_semaphore_take_blocking(ctx->cm_param.semaphore);

	enum router_result_status result = router_route_bundle(bundle);

	hal_semaphore_release(ctx->cm_param.semaphore);

	if (result == ROUTER_RESULT_OK) {
		/* 5.4-4 */
		/* We do not accept custody -> only inform CM */
		wake_up_contact_manager(
			ctx->cm_param.control_queue,
			CM_SIGNAL_PROCESS_CURRENT_BUNDLES
		);
		return UD3TN_OK;
	}

	LOGF("BundleProcessor: Routing bundle %p failed: %s",
		bundle,
		get_router_status_str(result));
	if (result == ROUTER_RESULT_EXPIRED)
		bundle_expired(ctx, bundle);
	else
		bundle_forwarding_contraindicated(
			ctx,
			bundle,
			get_fail_reason(result)
		);

	return UD3TN_FAIL;
}

/**
 * 4.3.4. Hop Count (BPv7-bis)
 *
 * Checks if the hop limit exceeds the hop limit. If yes, the bundle gets
 * deleted and false is returned. Otherwise the hop count is incremented
 * and true is returned.
 *
 *
 * @return false if the hop count exeeds the hop limit, true otherwise
 */
static bool hop_count_validation(struct bundle *bundle)
{
	struct bundle_block *block = bundle_block_find_first_by_type(
		bundle->blocks, BUNDLE_BLOCK_TYPE_HOP_COUNT);

	/* No Hop Count block was found */
	if (block == NULL)
		return true;

	struct bundle_hop_count hop_count;
	bool success = bundle7_hop_count_parse(&hop_count,
		block->data, block->length);

	/* If block data cannot be parsed, ignore it */
	if (!success) {
		LOGF("BundleProcessor: Could not parse hop-count block of bundle %p.",
			bundle);
		return true;
	}

	/* Hop count exceeded */
	if (hop_count.count >= hop_count.limit)
		return false;

	/* Increment Hop Count */
	hop_count.count++;

	/* CBOR-encoding */
	uint8_t *buffer = malloc(BUNDLE7_HOP_COUNT_MAX_ENCODED_SIZE);

	/* Out of memory - validation passes none the less */
	if (buffer == NULL) {
		LOGF("BundleProcessor: Could not increment hop-count of bundle %p.",
			bundle);
		return true;
	}

	free(block->data);

	block->data = buffer;
	block->length = bundle7_hop_count_serialize(&hop_count,
		buffer, BUNDLE7_HOP_COUNT_MAX_ENCODED_SIZE);

	return true;
}

/**
 * Get the agent identifier for local bundle delivery.
 * The agent identifier should follow the local EID behind a slash ('/').
 */
static const char *get_agent_id(const struct bp_context *const ctx, const char *dest_eid)
{
	const size_t local_len = strlen(ctx->local_eid_prefix);
	const size_t dest_len = strlen(dest_eid);

	if (dest_len <= local_len)
		return NULL;
	// `ipn` EIDs always end with ".0", prefix ends with "."
	// -> agent starts after local_len
	if (ctx->local_eid_is_ipn) {
		if (dest_eid[local_len - 1] != '.')
			return NULL;
		return &dest_eid[local_len];
	}
	// The local `dtn` EID prefix never ends with a '/', it follows after
	// (see bundle_processor_task); the `<=` above protects this check
	if (dest_eid[local_len] != '/')
		return NULL;
	return &dest_eid[local_len + 1];
}

// Checks whether we know the bundle. If not, adds it to the list.
static bool bundle_record_add_and_check_known(
	struct bp_context *const ctx, const struct bundle *bundle)
{
	struct known_bundle_list **cur_entry = &ctx->known_bundle_list;
	uint64_t cur_time_ms = hal_time_get_timestamp_ms();
	const uint64_t bundle_deadline_ms = bundle_get_expiration_time_ms(
		bundle
	);

	if (bundle_deadline_ms < cur_time_ms)
		return true; // We assume we "know" all expired bundles.
	// 1. Cleanup and search
	while (*cur_entry != NULL) {
		struct known_bundle_list *e = *cur_entry;

		if (bundle_is_equal(bundle, &e->id)) {
			return true;
		} else if (e->deadline_ms < cur_time_ms) {
			*cur_entry = e->next;
			bundle_free_unique_identifier(&e->id);
			free(e);
			continue;
		} else if (e->deadline_ms > bundle_deadline_ms) {
			// Won't find, insert here!
			break;
		}
		cur_entry = &(*cur_entry)->next;
	}

	// 2. If not found, add at current slot (ordered by deadline)
	struct known_bundle_list *new_entry = malloc(
		sizeof(struct known_bundle_list)
	);

	if (!new_entry)
		return false;
	new_entry->id = bundle_get_unique_identifier(bundle);
	new_entry->deadline_ms = bundle_deadline_ms;
	new_entry->next = *cur_entry;
	*cur_entry = new_entry;

	return false;
}

static bool bundle_reassembled_is_known(
	struct bp_context *const ctx, const struct bundle *bundle)
{
	struct known_bundle_list **cur_entry = &ctx->known_bundle_list;
	const uint64_t bundle_deadline_ms = bundle_get_expiration_time_ms(
		bundle
	);

	while (*cur_entry != NULL) {
		struct known_bundle_list *e = *cur_entry;

		if (bundle_is_equal_parent(bundle, &e->id) &&
				e->id.fragment_offset == 0 &&
				e->id.payload_length ==
					bundle->total_adu_length) {
			return true;
		} else if (e->deadline_ms > bundle_deadline_ms) {
			// Won't find...
			break;
		}
		cur_entry = &(*cur_entry)->next;
	}
	return false;
}

static void bundle_add_reassembled_as_known(
	struct bp_context *const ctx, const struct bundle *bundle)
{
	struct known_bundle_list **cur_entry = &ctx->known_bundle_list;
	const uint64_t bundle_deadline_ms = bundle_get_expiration_time_ms(
		bundle
	);

	while (*cur_entry != NULL) {
		struct known_bundle_list *e = *cur_entry;

		if (e->deadline_ms > bundle_deadline_ms)
			break;
		cur_entry = &(*cur_entry)->next;
	}

	struct known_bundle_list *new_entry = malloc(
		sizeof(struct known_bundle_list)
	);

	if (!new_entry)
		return;
	new_entry->id = bundle_get_unique_identifier(bundle);
	new_entry->id.fragment_offset = 0;
	new_entry->id.payload_length = bundle->total_adu_length;
	new_entry->deadline_ms = bundle_deadline_ms;
	new_entry->next = *cur_entry;
	*cur_entry = new_entry;
}

// Interaction with CM / RT

// NOTE: This never blocks to prevent deadlocks.
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

static void bundle_resched_func(struct bundle *bundle, const void *ctx)
{
	const struct bp_context *bp_context = ctx;

	bundle_dangling(bp_context, bundle);
}
