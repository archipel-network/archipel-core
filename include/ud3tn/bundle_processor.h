#ifndef BUNDLEPROCESSOR_H_INCLUDED
#define BUNDLEPROCESSOR_H_INCLUDED

#include "ud3tn/bundle.h"

#include "platform/hal_types.h"

enum bundle_processor_signal_type {
	BP_SIGNAL_BUNDLE_INCOMING,
	BP_SIGNAL_BUNDLE_ROUTED,
	BP_SIGNAL_FORWARDING_CONTRAINDICATED,
	BP_SIGNAL_BUNDLE_EXPIRED,
	BP_SIGNAL_RESCHEDULE_BUNDLE,
	BP_SIGNAL_TRANSMISSION_SUCCESS,
	BP_SIGNAL_TRANSMISSION_FAILURE,
	BP_SIGNAL_BUNDLE_LOCAL_DISPATCH,
	BP_SIGNAL_AGENT_REGISTER,
	BP_SIGNAL_AGENT_DEREGISTER
};

struct bundle_processor_signal {
	enum bundle_processor_signal_type type;
	enum bundle_status_report_reason reason;
	bundleid_t bundle;
	void *extra;
};

struct bundle_processor_task_parameters {
	QueueIdentifier_t router_signaling_queue;
	QueueIdentifier_t signaling_queue;
	const char *local_eid;
	bool status_reporting;
};

void bundle_processor_inform(
	QueueIdentifier_t bundle_processor_signaling_queue, bundleid_t bundle,
	enum bundle_processor_signal_type type,
	enum bundle_status_report_reason reason);


/**
 * @brief Instruct the BP to interact with the agent manager state
 *
 * @param bundle_processor_signaling_queue Handle to the signaling queue of
 *	the BP task
 * @param type BP_SIGNAL_AGENT_REGISTER or BP_SIGNAL_AGENT_DRREGISTER
 * @param sink_identifier Unique string to identify an agent, must not be NULL
 * @param callback Logic to be executed every time a bundle should be
 *	delivered to the agent (BP_SIGNAL_AGENT_REGISTER only)
 * @param param Use this to pass additional arguments to callback
 *	(BP_SIGNAL_AGENT_REGISTER only)
 * @param wait_for_feedback If true, block and wait for the feedback of the BP
 */
int bundle_processor_perform_agent_action(
	QueueIdentifier_t bundle_processor_signaling_queue,
	enum bundle_processor_signal_type type,
	const char *sink_identifier,
	void (*const callback)(struct bundle_adu data, void *param),
	void *param,
	bool wait_for_feedback);


void bundle_processor_task(void *param);

#endif /* BUNDLEPROCESSOR_H_INCLUDED */
