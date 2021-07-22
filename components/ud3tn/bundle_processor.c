#include "ud3tn/agent_manager.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/custody_manager.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/bundle_storage_manager.h"
#include "ud3tn/report_manager.h"
#include "ud3tn/result.h"
#include "ud3tn/router_task.h"
#include "ud3tn/task_tags.h"

#include "bundle6/bundle6.h"
#include "bundle7/bundle_age.h"
#include "bundle7/hopcount.h"

#include "platform/hal_io.h"
#include "platform/hal_queue.h"
#include "platform/hal_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum bundle_handling_result {
	BUNDLE_HRESULT_OK = 0,
	BUNDLE_HRESULT_DELETED,
	BUNDLE_HRESULT_BLOCK_DISCARDED,
};

// TODO: Move static state into context struct passed into functions

static QueueIdentifier_t out_queue;
static const char *local_eid;
static bool status_reporting;

static struct reassembly_list {
	struct reassembly_bundle_list {
		struct bundle *bundle;
		struct reassembly_bundle_list *next;
	} *bundle_list;
	struct reassembly_list *next;
} *reassembly_list;

static struct known_bundle_list {
	struct bundle_unique_identifier id;
	uint64_t deadline;
	struct known_bundle_list *next;
} *known_bundle_list;

// for performing (de)register operations
struct agent_manager_parameters {
	QueueIdentifier_t feedback_queue;
	struct agent agent;
};

/* DECLARATIONS */

static inline void handle_signal(const struct bundle_processor_signal signal);

static void bundle_dispatch(struct bundle *bundle);
static bool bundle_endpoint_is_local(struct bundle *bundle);
static enum ud3tn_result bundle_forward(struct bundle *bundle, uint16_t timeout);
static void bundle_forwarding_scheduled(struct bundle *bundle);
static void bundle_forwarding_success(struct bundle *bundle);
static void bundle_forwarding_contraindicated(
	struct bundle *bundle, enum bundle_status_report_reason reason);
static void bundle_forwarding_failed(
	struct bundle *bundle, enum bundle_status_report_reason reason);
static void bundle_expired(struct bundle *bundle);
static void bundle_receive(struct bundle *bundle);
static enum bundle_handling_result handle_unknown_block_flags(
	struct bundle *bundle, enum bundle_block_flags flags);
static void bundle_deliver_local(struct bundle *bundle);
static void bundle_attempt_reassembly(struct bundle *bundle);
static void bundle_deliver_adu(struct bundle_adu data);
static void bundle_custody_accept(struct bundle *bundle);
static void bundle_custody_success(struct bundle *bundle);
static void bundle_custody_failure(
	struct bundle *bundle, enum bundle_custody_signal_reason reason);
static void bundle_delete(
	struct bundle *bundle, enum bundle_status_report_reason reason);
static void bundle_discard(struct bundle *bundle);
static void bundle_handle_custody_signal(
	struct bundle_administrative_record *signal);
static void bundle_dangling(struct bundle *bundle);
static bool hop_count_validation(struct bundle *bundle);
static const char *get_agent_id(const char *dest_eid);
static bool bundle_record_add_and_check_known(const struct bundle *bundle);
static bool bundle_reassembled_is_known(const struct bundle *bundle);
static void bundle_add_reassembled_as_known(const struct bundle *bundle);

static void send_status_report(
	struct bundle *bundle,
	const enum bundle_status_report_status_flags status,
	const enum bundle_status_report_reason reason);
static void send_custody_signal(struct bundle *bundle,
	const enum bundle_custody_signal_type,
	const enum bundle_custody_signal_reason reason);
static enum ud3tn_result send_bundle(bundleid_t bundle, uint16_t timeout);

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

/* COMMUNICATION */

void bundle_processor_inform(
	QueueIdentifier_t bundle_processor_signaling_queue, bundleid_t bundle,
	enum bundle_processor_signal_type type,
	enum bundle_status_report_reason reason)
{
	struct bundle_processor_signal signal = {
		.type = type,
		.reason = reason,
		.bundle = bundle,
		.extra = NULL
	};

	ASSERT(type != BP_SIGNAL_AGENT_REGISTER && type != BP_SIGNAL_AGENT_DEREGISTER);

	hal_queue_push_to_back(bundle_processor_signaling_queue, &signal);
}

int bundle_processor_perform_agent_action(
	QueueIdentifier_t signaling_queue,
	enum bundle_processor_signal_type type,
	const char *sink_identifier,
	void (*const callback)(struct bundle_adu data, void *param),
	void *param,
	bool wait_for_feedback)
{
	struct bundle_processor_signal signal = {
		.type = type,
		.reason = BUNDLE_SR_REASON_NO_INFO,
		.bundle = BUNDLE_INVALID_ID,
		.extra = NULL
	};

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
	signal.extra = aaps;

	if (!wait_for_feedback) {
		hal_queue_push_to_back(signaling_queue, &signal);
		return 0;
	}

	feedback_queue = hal_queue_create(1, sizeof(int));
	if (!feedback_queue) {
		free(aaps);
		return -1;
	}

	aaps->feedback_queue = feedback_queue;
	hal_queue_push_to_back(signaling_queue, &signal);

	if (hal_queue_receive(feedback_queue, &result, -1) == UD3TN_OK) {
		hal_queue_delete(feedback_queue);
		return result;
	}

	hal_queue_delete(feedback_queue);
	return -1;
}

void bundle_processor_task(void * const param)
{
	struct bundle_processor_task_parameters *p =
		(struct bundle_processor_task_parameters *)param;
	struct bundle_processor_signal signal;

	out_queue = p->router_signaling_queue;
	local_eid = p->local_eid;
	status_reporting = p->status_reporting;

	custody_manager_init(p->local_eid);

	LOGF("BundleProcessor: BPA initialized for \"%s\", status reports %s",
	     p->local_eid, p->status_reporting ? "enabled" : "disabled");

	for (;;) {
		if (hal_queue_receive(p->signaling_queue, &signal,
			-1) == UD3TN_OK
		) {
			handle_signal(signal);
		}
	}
}

static inline void handle_signal(const struct bundle_processor_signal signal)
{
	bool bundle_required = signal.type != BP_SIGNAL_AGENT_REGISTER &&
		signal.type != BP_SIGNAL_AGENT_DEREGISTER;
	struct bundle *b = bundle_storage_get(signal.bundle);
	struct agent_manager_parameters *aaps;
	int feedback;

	if (bundle_required && b == NULL) {
		LOGI("BundleProcessor: Could not process signal", signal.type);
		LOGI("BundleProcessor: Bundle not found", signal.bundle);
		return;
	}

	switch (signal.type) {
	case BP_SIGNAL_BUNDLE_INCOMING:
		bundle_receive(b);
		break;
	case BP_SIGNAL_BUNDLE_ROUTED:
		bundle_forwarding_scheduled(b);
		break;
	case BP_SIGNAL_FORWARDING_CONTRAINDICATED:
		bundle_forwarding_contraindicated(b, signal.reason);
		break;
	case BP_SIGNAL_BUNDLE_EXPIRED:
		bundle_expired(b);
		break;
	case BP_SIGNAL_RESCHEDULE_BUNDLE:
		bundle_dangling(b);
		break;
	case BP_SIGNAL_TRANSMISSION_SUCCESS:
		bundle_forwarding_success(b);
		break;
	case BP_SIGNAL_TRANSMISSION_FAILURE:
		bundle_forwarding_failed(b,
			BUNDLE_SR_REASON_TRANSMISSION_CANCELED);
		break;
	case BP_SIGNAL_BUNDLE_LOCAL_DISPATCH:
		bundle_dispatch(b);
		break;
	case BP_SIGNAL_AGENT_REGISTER:
		aaps = ((struct agent_manager_parameters *) signal.extra);
		feedback = agent_register(aaps->agent.sink_identifier,
			aaps->agent.callback, aaps->agent.param);
		if (aaps->feedback_queue)
			hal_queue_push_to_back(aaps->feedback_queue, &feedback);
		free(aaps);
		break;
	case BP_SIGNAL_AGENT_DEREGISTER:
		aaps = ((struct agent_manager_parameters *) signal.extra);
		feedback = agent_deregister(aaps->agent.sink_identifier);
		if (aaps->feedback_queue)
			hal_queue_push_to_back(aaps->feedback_queue, &feedback);
		free(aaps);
		break;
	default:
		LOGF("BundleProcessor: Invalid signal (%d) detected",
		     signal.type);
		break;
	}
	/* bundle_storage_update(b); */
}

/* BUNDLE HANDLING */

/* 5.3 */
static void bundle_dispatch(struct bundle *bundle)
{
	/* 5.3-1 */
	if (bundle_endpoint_is_local(bundle)) {
		bundle_deliver_local(bundle);
		return;
	}
	/* 5.3-2 */
	bundle_forward(bundle, 0);
}

/* 5.3-1 */
static bool bundle_endpoint_is_local(struct bundle *bundle)
{
	const size_t local_len = strlen(local_eid);
	const size_t dest_len = strlen(bundle->destination);

	/* Compare bundle destination EID _prefix_ with configured uD3TN EID */
	return (
		// For the memcmp to be safe, the destination EID has to be at
		// least as long as the local EID.
		dest_len >= local_len &&
		// The prefix (the local EID) has to match the bundle dest-EID.
		memcmp(local_eid, bundle->destination, local_len) == 0
	);
}

/* 5.4 */
static enum ud3tn_result bundle_forward(struct bundle *bundle, uint16_t timeout)
{
	/* 4.3.4. Hop Count (BPv7-bis) */
	/* TODO: Is this the correct point to perform the hop-count check? */
	if (!hop_count_validation(bundle)) {
		bundle_delete(bundle, BUNDLE_SR_REASON_HOP_LIMIT_EXCEEDED);
		return UD3TN_FAIL;
	}

	/* 5.4-1 */
	bundle_add_rc(bundle, BUNDLE_RET_CONSTRAINT_FORWARD_PENDING);
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING, 0);
	/* 5.4-2 */
	if (send_bundle(bundle->id, timeout) != UD3TN_OK) {
		/* Could not store bundle in queue -> delete it. */
		bundle_delete(bundle, BUNDLE_SR_REASON_DEPLETED_STORAGE);
		return UD3TN_FAIL;
	}
	/* For steps after 5.4-2, see below */
	return UD3TN_OK;
}

/* 5.4-4 */
static void bundle_forwarding_scheduled(struct bundle *bundle)
{
	/* 5.4-4 */
	/* Custody may have already been accepted if we are re-scheduling */
	if (
		HAS_FLAG(bundle->proc_flags,
			 BUNDLE_V6_FLAG_CUSTODY_TRANSFER_REQUESTED)
		&& !HAS_FLAG(bundle->ret_constraints,
			BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED)
		&& bundle->protocol_version == 6
	) {
		/* bundle_receive already checked if bundle is acceptable */
		bundle_custody_accept(bundle);
	}
	/* 5.4-5 is done by contact manager / ground station task */
}

/* 5.4-6 */
static void bundle_forwarding_success(struct bundle *bundle)
{
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_REPORT_FORWARDING)) {
		/* See 5.4-6: reason code vs. unidirectional links */
		send_status_report(bundle,
			BUNDLE_SR_FLAG_BUNDLE_FORWARDED,
			BUNDLE_SR_REASON_NO_INFO);
	}
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_FORWARD_PENDING, 0);
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_FLAG_OWN, 1);
}

/* 5.4.1 */
static void bundle_forwarding_contraindicated(
	struct bundle *bundle, enum bundle_status_report_reason reason)
{
	/* 5.4.1-1: For now, we declare forwarding failure everytime */
	bundle_forwarding_failed(bundle, reason);
	/* 5.4.1-2 (a): At the moment, custody transfer is declared as failed */
	/* 5.4.1-2 (b): Will not be handled */
}

/* 5.4.2 */
static void bundle_forwarding_failed(
	struct bundle *bundle, enum bundle_status_report_reason reason)
{
	enum bundle_custody_signal_reason cs_reason;

	if (HAS_FLAG(bundle->proc_flags,
		BUNDLE_V6_FLAG_CUSTODY_TRANSFER_REQUESTED)
	) {
		if (HAS_FLAG(bundle->ret_constraints,
			BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED)
		) {
			/* TODO: See 5.12, evaluate what to do here */
			/* We are the current custodian and... */
			/* a) were re-scheduling but it wasn't possible or */
			/* b) the GS task failed */
		} else {
			/* We tried to schedule the bundle but it failed */
			cs_reason = (reason < BUNDLE_SR_REASON_DEPLETED_STORAGE
					? BUNDLE_CS_REASON_NO_INFO
					: (enum bundle_custody_signal_reason)
						reason);
			send_custody_signal(bundle, BUNDLE_CS_TYPE_REFUSAL,
				cs_reason);
		}
	}
	bundle_delete(bundle, reason);
}

/* 5.5 */
static void bundle_expired(struct bundle *bundle)
{
	bundle_delete(bundle, BUNDLE_SR_REASON_LIFETIME_EXPIRED);
}

/* 5.6 */
static void bundle_receive(struct bundle *bundle)
{
	struct bundle_block_list **e;
	enum bundle_handling_result res;

	// Set the reception time to calculate the bundle's residence time
	bundle->reception_timestamp_ms = hal_time_get_timestamp_ms();

	/* 5.6-1 Add retention constraint */
	bundle_add_rc(bundle, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING);
	/* 5.6-2 Request reception */
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_REPORT_RECEPTION))
		send_status_report(bundle,
			BUNDLE_SR_FLAG_BUNDLE_RECEIVED,
			BUNDLE_SR_REASON_NO_INFO);

	// Check lifetime
	if (bundle_get_expiration_time_s(bundle) < hal_time_get_timestamp_s()) {
		bundle_delete(bundle, BUNDLE_SR_REASON_LIFETIME_EXPIRED);
		return;
	}

	/* 5.6-3 Handle blocks */
	e = &bundle->blocks;
	while (*e != NULL) {
		if ((*e)->data->type != BUNDLE_BLOCK_TYPE_PAYLOAD) {
			res = handle_unknown_block_flags(
				bundle, (*e)->data->flags);
			switch (res) {
			case BUNDLE_HRESULT_OK:
				(*e)->data->flags |=
					BUNDLE_V6_BLOCK_FLAG_FWD_UNPROC;
				break;
			case BUNDLE_HRESULT_DELETED:
				bundle_delete(bundle,
					BUNDLE_SR_REASON_BLOCK_UNINTELLIGIBLE);
				return;
			case BUNDLE_HRESULT_BLOCK_DISCARDED:
				*e = bundle_block_entry_free(*e);
				break;
			}

		}
		if (*e != NULL)
			e = &(*e)->next;
	}
	/* Test for custody acceptance */
	if (HAS_FLAG(bundle->proc_flags,
		     BUNDLE_V6_FLAG_CUSTODY_TRANSFER_REQUESTED)
		&& bundle->protocol_version == 6
	) {
		if (custody_manager_has_redundant_bundle(bundle)) {
			/* 5.6-4 */
			send_custody_signal(bundle, BUNDLE_CS_TYPE_REFUSAL,
				BUNDLE_CS_REASON_REDUNDANT_RECEPTION);
			bundle_rem_rc(bundle,
				BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING, 1);
		} else if (!custody_manager_storage_is_acceptable(bundle)) {
			/* This is not part of the specification, but we have */
			/* to check if we want to reject custody before */
			/* dispatching the bundle. */
			/* In that case, the bundle would be deleted. */
			send_custody_signal(bundle, BUNDLE_CS_TYPE_REFUSAL,
				BUNDLE_CS_REASON_DEPLETED_STORAGE);
			bundle_delete(bundle,
				BUNDLE_SR_REASON_DEPLETED_STORAGE);
		}
	} else {
		/* 5.6-5 */
		bundle_dispatch(bundle);
	}
}

/* 5.6-3 */
static enum bundle_handling_result handle_unknown_block_flags(
	struct bundle *bundle, enum bundle_block_flags flags)
{
	if (HAS_FLAG(flags, BUNDLE_BLOCK_FLAG_REPORT_IF_UNPROC)) {
		send_status_report(bundle,
			BUNDLE_SR_FLAG_BUNDLE_RECEIVED,
			BUNDLE_SR_REASON_BLOCK_UNINTELLIGIBLE);
	}
	if (HAS_FLAG(flags, BUNDLE_BLOCK_FLAG_DELETE_BUNDLE_IF_UNPROC))
		return BUNDLE_HRESULT_DELETED;
	else if (HAS_FLAG(flags, BUNDLE_BLOCK_FLAG_DISCARD_IF_UNPROC))
		return BUNDLE_HRESULT_BLOCK_DISCARDED;
	return BUNDLE_HRESULT_OK;
}

/* 5.7 */
static void bundle_deliver_local(struct bundle *bundle)
{
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING, 0);

	/* Check and record knowledge of bundle */
	if (bundle_record_add_and_check_known(bundle)) {
		LOGF("Bundle #%d was already delivered, dropping it",
		     bundle->id);
		// NOTE: We cannot have custody as the CM checks for duplicates
		bundle_discard(bundle);
		return;
	}

	/* Report successful delivery, if applicable */
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_REPORT_DELIVERY)) {
		send_status_report(bundle,
			BUNDLE_SR_FLAG_BUNDLE_DELIVERED,
			BUNDLE_SR_REASON_NO_INFO);
	}

	if (!HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_ADMINISTRATIVE_RECORD) &&
			get_agent_id(bundle->destination) == NULL) {
		// If it is no admin. record and we have no agent to deliver#
		// it to, drop it.
		LOGF("BundleProcessor: Received bundle not destined for any registered EID (from = %s, to = %s), dropping...",
		     bundle->source, bundle->destination);
		bundle_delete(bundle, BUNDLE_SR_REASON_DEST_EID_UNINTELLIGIBLE);
		return;
	}

	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_IS_FRAGMENT)) {
		bundle_add_rc(bundle, BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING);
		bundle_attempt_reassembly(bundle);
	} else {
		struct bundle_adu adu = bundle_to_adu(bundle);

		bundle_discard(bundle);
		bundle_deliver_adu(adu);
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

static void add_to_reassembly_bundle_list(struct reassembly_list *item,
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
		bundle_delete(bundle, BUNDLE_SR_REASON_DEPLETED_STORAGE);
		return;
	}
	new_entry->bundle = bundle;
	new_entry->next = *cur_entry;
	*cur_entry = new_entry;
}

static void try_reassemble(struct reassembly_list **slot)
{
	struct reassembly_list *const e = *slot;
	struct reassembly_bundle_list *eb;
	struct bundle *b;

	size_t pos_in_bundle = 0;

	LOG("Attempting bundle reassembly!");

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
	LOG("Reassembling bundle!");

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
			bundle_add_reassembled_as_known(b);
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
	bundle_deliver_adu(adu);
}

static void bundle_attempt_reassembly(struct bundle *bundle)
{
	struct reassembly_list **r_list_e = &reassembly_list;

	if (bundle_reassembled_is_known(bundle)) {
		LOGF("Original bundle for #%d was already delivered, dropping",
		     bundle->id);
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
			add_to_reassembly_bundle_list(e, bundle);
			try_reassemble(r_list_e);
			return;
		}
	}

	// Not found, append
	struct reassembly_list *new_list = malloc(
		sizeof(struct reassembly_list)
	);

	if (!new_list) {
		bundle_delete(bundle, BUNDLE_SR_REASON_DEPLETED_STORAGE);
		return;
	}
	new_list->bundle_list = NULL;
	new_list->next = NULL;
	add_to_reassembly_bundle_list(new_list, bundle);
	*r_list_e = new_list;
	try_reassemble(r_list_e);
}

static void bundle_deliver_adu(struct bundle_adu adu)
{
	struct bundle_administrative_record *record;

	if (HAS_FLAG(adu.proc_flags, BUNDLE_FLAG_ADMINISTRATIVE_RECORD)) {
		record = parse_administrative_record(
			adu.protocol_version,
			adu.payload,
			adu.length
		);
		if (record != NULL && record->type == BUNDLE_AR_CUSTODY_SIGNAL)
			bundle_handle_custody_signal(record);
		free_administrative_record(record);
		bundle_adu_free_members(adu);
		return;
	}

	const char *agent_id = get_agent_id(adu.destination);

	ASSERT(agent_id != NULL);
	LOGF("BundleProcessor: Received local bundle -> \"%s\"; len(PL) = %d B",
	     agent_id, adu.length);
	agent_forward(agent_id, adu);
}

/* 5.10 */
static void bundle_custody_accept(struct bundle *bundle)
{
	if (custody_manager_accept(bundle) != UD3TN_OK) {
		/* TODO */
		return;
	}

	if (HAS_FLAG(bundle->proc_flags,
		BUNDLE_V6_FLAG_REPORT_CUSTODY_ACCEPTANCE)
	) {
		/* TODO: 5.10.1: */
		/* However, if a bundle reception status report was */
		/* generated for this bundle (Step 1 of Section 5.6), */
		/* then this report should be generated by simply turning on */
		/* the "Reporting node accepted custody of bundle" flag */
		/* in that earlier report's status flags byte. */
		send_status_report(bundle, BUNDLE_SR_FLAG_CUSTODY_TRANSFER,
			BUNDLE_SR_REASON_NO_INFO);
	}
	send_custody_signal(bundle, BUNDLE_CS_TYPE_ACCEPTANCE,
		BUNDLE_CS_REASON_NO_INFO);
}

/* 5.11 */
static void bundle_custody_success(struct bundle *bundle)
{
	/* 5.10.2 */
	custody_manager_release(bundle);
}

/* 5.12 */
/* "Custody failed" signal received or timer expired (TODO) */
static void bundle_custody_failure(
	struct bundle *bundle, enum bundle_custody_signal_reason reason)
{
	switch (reason) {
		/* TODO: Which reports should be generated? */
	default:
		custody_manager_release(bundle);
		break;
	}
}

/* 5.13 (BPv7) */
/* TODO: Custody Transfer Deferral */

/* 5.13 (RFC 5050) */
/* 5.14 (BPv7-bis) */
static void bundle_delete(
	struct bundle *bundle, enum bundle_status_report_reason reason)
{
	bool generate_report = false;

	if (HAS_FLAG(bundle->ret_constraints,
		BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED)
	) {
		custody_manager_release(bundle);
		generate_report = true;
	} else if (HAS_FLAG(bundle->proc_flags,
		BUNDLE_FLAG_REPORT_DELETION)) {
		generate_report = true;
	}

	if (generate_report)
		send_status_report(bundle,
			BUNDLE_SR_FLAG_BUNDLE_DELETED, reason);

	bundle->ret_constraints &= BUNDLE_RET_CONSTRAINT_NONE;
	bundle_discard(bundle);
}

/* 5.14 (RFC 5050) */
/* 5.15 (BPv7-bis) */
static void bundle_discard(struct bundle *bundle)
{
	bundle_storage_delete(bundle->id);
	bundle_drop(bundle);
}

/* 6.3 */
static void bundle_handle_custody_signal(
	struct bundle_administrative_record *signal)
{
	struct bundle *bundle = custody_manager_get_by_record(signal);

	if (bundle == NULL)
		return;
	if (signal->custody_signal->type == BUNDLE_CS_TYPE_ACCEPTANCE)
		bundle_custody_success(bundle);
	else
		bundle_custody_failure(bundle, signal->custody_signal->reason);
}

/* RE-SCHEDULING */
static void bundle_dangling(struct bundle *bundle)
{
	uint8_t resched = 0;

	switch (FAILED_FORWARD_POLICY) {
	case POLICY_TRY_RE_SCHEDULE:
		resched = 1;
		break;
	case POLICY_DROP_IF_NO_CUSTODY:
		resched = (HAS_FLAG(bundle->ret_constraints,
				BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED) ||
			HAS_FLAG(bundle->ret_constraints,
				BUNDLE_RET_CONSTRAINT_FLAG_OWN));
		break;
	}
	if (!resched) {
		bundle_delete(bundle, BUNDLE_SR_REASON_TRANSMISSION_CANCELED);
	/* Send it to the router task again after evaluating policy. */
	} else if (send_bundle(bundle->id, FAILED_FORWARD_TIMEOUT) != UD3TN_OK) {
		LOGF("Failed forwarding bundle #%d to router task, dropping.",
		     bundle->id);
		bundle_delete(bundle, BUNDLE_SR_REASON_DEPLETED_STORAGE);
	}
}

/* HELPERS */

static void send_status_report(
	struct bundle *bundle,
	const enum bundle_status_report_status_flags status,
	const enum bundle_status_report_reason reason)
{
	if (!status_reporting)
		return;

	/* If the report-to EID is the null endpoint or uD3TN itself we do */
	/* not need to create a status report */
	if (strcmp(bundle->destination, "dtn:none") == 0
		|| strcmp(bundle->destination, local_eid) == 0)
		return;

	struct bundle_status_report report = {
		.status = status,
		.reason = reason
	};
	struct bundle *b = generate_status_report(bundle, &report, local_eid);

	if (b != NULL) {
		bundle_add_rc(b, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING);
		bundle_storage_add(b);
		if (bundle_forward(b, STATUS_REPORT_TIMEOUT) != UD3TN_OK)
			LOGF("Failed sending status report for bundle #%d.",
			     bundle->id);
	}
}

static void send_custody_signal(struct bundle *bundle,
	const enum bundle_custody_signal_type type,
	const enum bundle_custody_signal_reason reason)
{
	struct bundle_custody_signal signal = {
		.type = type,
		.reason = reason,
	};
	struct bundle_list *next;
	struct bundle_list *signals = generate_custody_signal(
		bundle,
		&signal,
		local_eid
	);

	/* Walk through all signals and forward them to their destination */
	while (signals != NULL) {
		bundle_add_rc(signals->data,
			BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING);
		bundle_storage_add(signals->data);
		if (bundle_forward(signals->data, CUSTODY_SIGNAL_TIMEOUT)
		    != UD3TN_OK)
			LOGF("Failed sending custody signal for bundle #%d.",
			     bundle->id);

		/* Free current list entry (but not the bundle itself) */
		next = signals->next;
		free(signals);
		signals = next;
	}
}

static enum ud3tn_result send_bundle(bundleid_t bundle, uint16_t timeout)
{
	struct router_signal signal = {
		.type = ROUTER_SIGNAL_ROUTE_BUNDLE,
		/* A little bit ugly but saves RAM... */
		.data = (void *)(uintptr_t)bundle
	};

	if (timeout == 0) {
		hal_queue_push_to_back(out_queue, &signal);
		return UD3TN_OK;
	}
	return hal_queue_try_push_to_back(out_queue,
					  &signal,
					  timeout);
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
		LOGI("BundleProcessor: Could not parse hop-count block",
			bundle->id);
		return true;
	}

	/* Hop count exeeded, delete bundle */
	if (hop_count.count >= hop_count.limit) {
		bundle_delete(bundle, BUNDLE_SR_REASON_HOP_LIMIT_EXCEEDED);
		return false;
	}

	/* Increment Hop Count */
	hop_count.count++;

	/* CBOR-encoding */
	uint8_t *buffer = malloc(BUNDLE7_HOP_COUNT_MAX_ENCODED_SIZE);

	/* Out of memory - validation passes none the less */
	if (buffer == NULL) {
		LOGI("BundleProcessor: Could not increment hop-count",
			bundle->id);
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
static const char *get_agent_id(const char *dest_eid)
{
	const size_t local_len = strlen(local_eid);
	const size_t dest_len = strlen(dest_eid);

	// Local EID ends with '/' -> agent starts at dest_eid[local_len]
	if (local_eid[local_len - 1] == '/') {
		if (dest_len <= local_len || dest_eid[local_len - 1] != '/')
			return NULL;
		return &dest_eid[local_len];
	}

	// Local EID does not end with '/' -> agent starts after local_len
	if (dest_len <= local_len + 1 || dest_eid[local_len] != '/')
		return NULL;
	return &dest_eid[local_len + 1];
}

// Checks whether we know the bundle. If not, adds it to the list.
static bool bundle_record_add_and_check_known(const struct bundle *bundle)
{
	struct known_bundle_list **cur_entry = &known_bundle_list;
	uint64_t cur_time = hal_time_get_timestamp_s();
	const uint64_t bundle_deadline = bundle_get_expiration_time_s(bundle);

	if (bundle_deadline < cur_time)
		return true; // We assume we "know" all expired bundles.
	// 1. Cleanup and search
	while (*cur_entry != NULL) {
		struct known_bundle_list *e = *cur_entry;

		if (bundle_is_equal(bundle, &e->id)) {
			return true;
		} else if (e->deadline < cur_time) {
			*cur_entry = e->next;
			bundle_free_unique_identifier(&e->id);
			free(e);
			continue;
		} else if (e->deadline > bundle_deadline) {
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
	new_entry->deadline = bundle_deadline;
	new_entry->next = *cur_entry;
	*cur_entry = new_entry;

	return false;
}

static bool bundle_reassembled_is_known(const struct bundle *bundle)
{
	struct known_bundle_list **cur_entry = &known_bundle_list;
	const uint64_t bundle_deadline = bundle_get_expiration_time_s(bundle);

	while (*cur_entry != NULL) {
		struct known_bundle_list *e = *cur_entry;

		if (bundle_is_equal_parent(bundle, &e->id) &&
				e->id.fragment_offset == 0 &&
				e->id.payload_length ==
					bundle->total_adu_length) {
			return true;
		} else if (e->deadline > bundle_deadline) {
			// Won't find...
			break;
		}
		cur_entry = &(*cur_entry)->next;
	}
	return false;
}

static void bundle_add_reassembled_as_known(const struct bundle *bundle)
{
	struct known_bundle_list **cur_entry = &known_bundle_list;
	const uint64_t bundle_deadline = bundle_get_expiration_time_s(bundle);

	while (*cur_entry != NULL) {
		struct known_bundle_list *e = *cur_entry;

		if (e->deadline > bundle_deadline)
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
	new_entry->deadline = bundle_deadline;
	new_entry->next = *cur_entry;
	*cur_entry = new_entry;
}
