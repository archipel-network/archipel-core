// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "agents/echo_agent.h"

#include "platform/hal_time.h"

#include "ud3tn/agent_util.h"
#include "ud3tn/bundle_agent_interface.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/eid.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct echo_agent_params {
	bool is_ipn;
	struct bundle_agent_interface *bai;
	uint64_t lifetime_s;

	uint64_t last_bundle_timestamp_s;
	uint64_t last_bundle_sequence_number;
};

static uint64_t allocate_sequence_number(
	struct echo_agent_params *const config,
	const uint64_t time_s)
{
	if (config->last_bundle_timestamp_s == time_s)
		return ++config->last_bundle_sequence_number;

	config->last_bundle_timestamp_s = time_s;
	config->last_bundle_sequence_number = 1;

	return 1;
}

static void callback(struct bundle_adu data, void *p)
{
	struct echo_agent_params *const params = p;

	const uint64_t time = hal_time_get_timestamp_s();
	const uint64_t seqnum = allocate_sequence_number(
		params,
		time
	);

	agent_create_forward_bundle(
		params->bai,
		data.protocol_version,
		params->is_ipn ? AGENT_ID_ECHO_IPN : AGENT_ID_ECHO_DTN,
		data.source,
		time,
		seqnum,
		params->lifetime_s,
		data.payload,
		data.length,
		0
	);

	// Pointer responsibility was taken by agent_create_forward_bundle
	data.payload = NULL;
	bundle_adu_free_members(data);
}

int echo_agent_setup(struct bundle_agent_interface *const bai,
		     const uint64_t lifetime_s)
{
	struct echo_agent_params *params = malloc(
		sizeof(struct echo_agent_params)
	);

	params->is_ipn = get_eid_scheme(bai->local_eid) == EID_SCHEME_IPN;
	params->bai = bai;
	params->lifetime_s = lifetime_s;
	params->last_bundle_timestamp_s = 0;
	params->last_bundle_sequence_number = 0;

	return bundle_processor_perform_agent_action(
		bai->bundle_signaling_queue,
		BP_SIGNAL_AGENT_REGISTER,
		params->is_ipn ? AGENT_ID_ECHO_IPN : AGENT_ID_ECHO_DTN,
		callback,
		params,
		false
	);
}
