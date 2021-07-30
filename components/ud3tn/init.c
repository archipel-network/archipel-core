#include "ud3tn/bundle_agent_interface.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/cmdline.h"
#include "ud3tn/common.h"
#include "ud3tn/init.h"
#include "ud3tn/router.h"
#include "ud3tn/router_task.h"
#include "ud3tn/task_tags.h"

#include "agents/application_agent.h"
#include "agents/config_agent.h"
#include "agents/management_agent.h"

#include "cla/cla.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_platform.h"
#include "platform/hal_queue.h"
#include "platform/hal_task.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static struct bundle_agent_interface bundle_agent_interface;

void init(int argc, char *argv[])
{
	hal_platform_init(argc, argv);
	LOG("INIT: uD3TN starting up...");
}

void start_tasks(const struct ud3tn_cmdline_options *const opt)
{
	if (!opt) {
		LOG("INIT: Error parsing options, terminating...");
		exit(EXIT_FAILURE);
	}

	if (opt->exit_immediately)
		exit(EXIT_SUCCESS);

	LOGF("INIT: Configured to use EID \"%s\" and BPv%d",
	     opt->eid, opt->bundle_version);

	if (opt->mbs) {
		struct router_config rc = router_get_config();

		if (opt->mbs <= SIZE_MAX)
			rc.global_mbs = (size_t)opt->mbs;
		router_update_config(rc);
	}

	bundle_agent_interface.local_eid = opt->eid;

	/* Initialize queues to communicate with the subsystems */
	bundle_agent_interface.router_signaling_queue
			= hal_queue_create(ROUTER_QUEUE_LENGTH,
					   sizeof(struct router_signal));
	ASSERT(bundle_agent_interface.router_signaling_queue != NULL);
	bundle_agent_interface.bundle_signaling_queue
			= hal_queue_create(BUNDLE_QUEUE_LENGTH,
				sizeof(struct bundle_processor_signal));
	ASSERT(bundle_agent_interface.bundle_signaling_queue != NULL);

	struct router_task_parameters *router_task_params =
			malloc(sizeof(struct router_task_parameters));
	ASSERT(router_task_params != NULL);
	router_task_params->router_signaling_queue =
			bundle_agent_interface.router_signaling_queue;
	router_task_params->bundle_processor_signaling_queue =
			bundle_agent_interface.bundle_signaling_queue;

	struct bundle_processor_task_parameters *bundle_processor_task_params
		= malloc(sizeof(struct bundle_processor_task_parameters));

	ASSERT(bundle_processor_task_params != NULL);
	bundle_processor_task_params->router_signaling_queue =
			bundle_agent_interface.router_signaling_queue;
	bundle_processor_task_params->signaling_queue =
			bundle_agent_interface.bundle_signaling_queue;
	bundle_processor_task_params->local_eid =
			bundle_agent_interface.local_eid;
	bundle_processor_task_params->status_reporting =
			opt->status_reporting;

	hal_task_create(router_task,
			"router_t",
			ROUTER_TASK_PRIORITY,
			router_task_params,
			DEFAULT_TASK_STACK_SIZE,
			(void *)ROUTER_TASK_TAG);

	hal_task_create(bundle_processor_task,
			"bundl_proc_t",
			BUNDLE_PROCESSOR_TASK_PRIORITY,
			bundle_processor_task_params,
			DEFAULT_TASK_STACK_SIZE,
			(void *)BUNDLE_PROCESSOR_TASK_TAG);

	config_agent_setup(bundle_agent_interface.bundle_signaling_queue,
		bundle_agent_interface.router_signaling_queue,
		bundle_agent_interface.local_eid);
	management_agent_setup(bundle_agent_interface.bundle_signaling_queue,
			       bundle_agent_interface.local_eid);

	const struct application_agent_config *aa_cfg = application_agent_setup(
		&bundle_agent_interface,
		opt->aap_socket,
		opt->aap_node,
		opt->aap_service,
		opt->bundle_version,
		opt->lifetime
	);

	if (!aa_cfg) {
		LOG("INIT: Application agent could not be initialized!");
		exit(EXIT_FAILURE);
	}

	/* Initialize the communication subsystem (CLA) */
	if (cla_initialize_all(opt->cla_options,
			       &bundle_agent_interface) != UD3TN_OK) {
		LOG("INIT: CLA subsystem could not be initialized!");
		exit(EXIT_FAILURE);
	}
}

__attribute__((noreturn))
int start_os(void)
{
	hal_task_start_scheduler();
	/* Should never get here! */
	ASSERT(0);
	hal_platform_restart();
	__builtin_unreachable();
}
