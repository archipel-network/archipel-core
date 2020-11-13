#ifndef BUNDLEPROCESSOR_H_INCLUDED
#define BUNDLEPROCESSOR_H_INCLUDED

#include "upcn/bundle.h"

#include "platform/hal_types.h"

enum bundle_processor_signal_type {
	BP_SIGNAL_BUNDLE_INCOMING,
	BP_SIGNAL_BUNDLE_ROUTED,
	BP_SIGNAL_FORWARDING_CONTRAINDICATED,
	BP_SIGNAL_BUNDLE_EXPIRED,
	BP_SIGNAL_RESCHEDULE_BUNDLE,
	BP_SIGNAL_TRANSMISSION_SUCCESS,
	BP_SIGNAL_TRANSMISSION_FAILURE,
	BP_SIGNAL_BUNDLE_LOCAL_DISPATCH
};

struct bundle_processor_signal {
	enum bundle_processor_signal_type type;
	enum bundle_status_report_reason reason;
	bundleid_t bundle;
};

struct bundle_processor_task_parameters {
	QueueIdentifier_t router_signaling_queue;
	QueueIdentifier_t signaling_queue;
	const char *local_eid;
	bool status_reporting;
};

void bundle_processor_inform(
	QueueIdentifier_t signaling_queue, bundleid_t bundle,
	enum bundle_processor_signal_type type,
	enum bundle_status_report_reason reason);
void bundle_processor_task(void *param);

#endif /* BUNDLEPROCESSOR_H_INCLUDED */
