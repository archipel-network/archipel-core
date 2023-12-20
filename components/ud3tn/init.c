// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/agent_manager.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/cmdline.h"
#include "ud3tn/common.h"
#include "ud3tn/init.h"
#include "ud3tn/router.h"

#include "aap2/aap2_agent.h"

#include "agents/application_agent.h"
#include "agents/echo_agent.h"

#include "cla/cla.h"

#include "platform/hal_io.h"
#include "platform/hal_platform.h"
#include "platform/hal_queue.h"
#include "platform/hal_task.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static struct bundle_agent_interface bundle_agent_interface;
uint8_t LOG_LEVEL = DEFAULT_LOG_LEVEL;

// References kept for program runtime
static struct application_agent_config *aa_cfg;
static struct aap2_agent_config *aa2_cfg;

void init(int argc, char *argv[])
{
	hal_platform_init(argc, argv);
	LOG_INFO("INIT: uD3TN starting up...");
}

void start_tasks(const struct ud3tn_cmdline_options *const opt)
{
	if (!opt) {
		LOG_ERROR("INIT: Error parsing options, terminating...");
		exit(EXIT_FAILURE);
	}

	if (opt->exit_immediately)
		exit(EXIT_SUCCESS);

	LOG_LEVEL = opt->log_level;

	LOGF_INFO("INIT: Configured to use EID \"%s\" and BPv%d",
	     opt->eid, opt->bundle_version);

	if (opt->mbs) {
		struct router_config rc = router_get_config();

		if (opt->mbs <= SIZE_MAX)
			rc.global_mbs = (size_t)opt->mbs;

		router_update_config(rc);
	}

	bundle_agent_interface.local_eid = opt->eid;

	/* Initialize queues to communicate with the subsystems */
	bundle_agent_interface.bundle_signaling_queue = hal_queue_create(
		BUNDLE_QUEUE_LENGTH,
		sizeof(struct bundle_processor_signal)
	);
	if (!bundle_agent_interface.bundle_signaling_queue) {
		LOG_ERROR("INIT: Allocation of `bundle_signaling_queue` failed");
		abort();
	}

	struct bundle_processor_task_parameters *bundle_processor_task_params =
		malloc(sizeof(struct bundle_processor_task_parameters));

	if (!bundle_processor_task_params) {
		LOG_ERROR("INIT: Allocation of `bundle_processor_task_params` failed");
		abort();
	}
	bundle_processor_task_params->signaling_queue =
			bundle_agent_interface.bundle_signaling_queue;
	bundle_processor_task_params->local_eid =
			bundle_agent_interface.local_eid;
	bundle_processor_task_params->status_reporting =
			opt->status_reporting;
	bundle_processor_task_params->allow_remote_configuration =
			opt->allow_remote_configuration;

	// NOTE: Must be called before launching the BP which calls the function
	// to register agents from its thread.
	agent_manager_init(bundle_agent_interface.local_eid);

	const enum ud3tn_result bp_task_result = hal_task_create(
		bundle_processor_task,
		bundle_processor_task_params
	);

	if (bp_task_result != UD3TN_OK) {
		LOG_ERROR("INIT: Bundle processor task could not be started!");
		abort();
	}

	int result = echo_agent_setup(
		&bundle_agent_interface,
		opt->lifetime_s * 1000
	);

	if (result) {
		LOG_ERROR("INIT: Echo agent could not be initialized!");
		abort();
	}

	if (opt->allow_remote_configuration)
		LOG_WARN("!! WARNING !! Remote configuration capability ENABLED!");

	aa_cfg = application_agent_setup(
		&bundle_agent_interface,
		opt->aap_socket,
		opt->aap_node,
		opt->aap_service,
		opt->bundle_version,
		opt->lifetime_s * 1000
	);

	if (!aa_cfg) {
		LOG_ERROR("INIT: Application agent could not be initialized!");
		abort();
	}

	aa2_cfg = aap2_agent_setup(
		&bundle_agent_interface,
		opt->aap2_socket,
		NULL,
		NULL,
		opt->bundle_version,
		opt->lifetime_s * 1000
	);

	if (!aa2_cfg) {
		LOG_ERROR("INIT: AAP2 agent could not be initialized!");
		abort();
	}

	/* Initialize the communication subsystem (CLA) */
	if (cla_initialize_all(opt->cla_options,
			       &bundle_agent_interface) != UD3TN_OK) {
		LOG_ERROR("INIT: CLA subsystem could not be initialized!");
		abort();
	}
}

__attribute__((noreturn))
int start_os(void)
{
	hal_task_start_scheduler();
	/* Should never get here! */
	abort();
}
