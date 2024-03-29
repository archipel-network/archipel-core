// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef BUNDLEPROCESSOR_H_INCLUDED
#define BUNDLEPROCESSOR_H_INCLUDED

#include "ud3tn/agent_manager.h"
#include "ud3tn/bundle.h"
#include "ud3tn/node.h"
#include "ud3tn/router.h"

#include "platform/hal_types.h"
#include "platform/hal_store.h"

// Contact dropping / failed forwarding policy
enum failed_forwarding_policy {
	POLICY_DROP,
	POLICY_TRY_RE_SCHEDULE
};

// Default policy for re-scheduling bundles on dropped contacts, etc.
#ifndef FAILED_FORWARD_POLICY
#define FAILED_FORWARD_POLICY POLICY_DROP
#endif // FAILED_FORWARD_POLICY

// Interface to the bundle agent, provided to other agents and the CLA.
struct bundle_agent_interface {
	char *local_eid;
	QueueIdentifier_t bundle_signaling_queue;
};

enum bundle_processor_signal_type {
	BP_SIGNAL_BUNDLE_INCOMING,
	BP_SIGNAL_TRANSMISSION_SUCCESS,
	BP_SIGNAL_TRANSMISSION_FAILURE,
	BP_SIGNAL_BUNDLE_LOCAL_DISPATCH,
	BP_SIGNAL_AGENT_REGISTER,
	BP_SIGNAL_AGENT_DEREGISTER,
	BP_SIGNAL_NEW_LINK_ESTABLISHED,
	BP_SIGNAL_LINK_DOWN,
	BP_SIGNAL_CONTACT_OVER,
	BP_SIGNAL_AGENT_REGISTER_RPC,
	BP_SIGNAL_AGENT_DEREGISTER_RPC,
};

// for performing (de)register operations
struct agent_manager_parameters {
	QueueIdentifier_t feedback_queue;
	struct agent agent;
};

struct bundle_processor_signal {
	enum bundle_processor_signal_type type;
	enum bundle_status_report_reason reason;
	struct bundle *bundle;
	char *peer_cla_addr;
	struct agent_manager_parameters *agent_manager_params;
	struct contact *contact;
	struct router_command *router_cmd;
};

struct bundle_processor_task_parameters {
	QueueIdentifier_t signaling_queue;
	const char *local_eid;
	bool status_reporting;
	bool allow_remote_configuration;
	#ifdef ARCHIPEL_CORE
	struct bundle_store* bundle_store;
	QueueIdentifier_t bundle_restore_queue;
	#endif
};

void bundle_processor_inform(
	QueueIdentifier_t bundle_processor_signaling_queue,
	const struct bundle_processor_signal signal);

/**
 * @brief Instruct the BP to interact with the agent manager state
 *
 * @param bundle_processor_signaling_queue Handle to the signaling queue of
 *	the BP task
 * @param type BP_SIGNAL_AGENT_REGISTER or BP_SIGNAL_AGENT_DRREGISTER
 * @param agent The agent parameters to be passed to the BP
 * @param wait_for_feedback If true, block and wait for the feedback of the BP
 */
int bundle_processor_perform_agent_action(
	QueueIdentifier_t signaling_queue,
	enum bundle_processor_signal_type type,
	const struct agent agent,
	bool wait_for_feedback);

// Forward declaration of internal opaque struct. Only to be used by agents
// from the BP task (not thread safe).
struct bp_context;

/**
 * @brief Dispatch a bundle - only to be executed from the BP thread.
 * @note Only to be used by agents from the BP task (not thread safe).
 */
enum ud3tn_result bundle_processor_bundle_dispatch(
	void *bp_context, struct bundle *bundle);

/**
 * @brief Process a router command - only to be executed by the config agent.
 * @note Only to be used by agents from the BP task (not thread safe).
 */
void bundle_processor_handle_router_command(
	void *bp_context, struct router_command *cmd);

void bundle_processor_task(void *param);

#endif /* BUNDLEPROCESSOR_H_INCLUDED */
