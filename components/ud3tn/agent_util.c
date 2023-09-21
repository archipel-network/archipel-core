// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "bundle6/create.h"
#include "bundle7/create.h"

#include "ud3tn/agent_util.h"
#include "ud3tn/common.h"
#include "ud3tn/bundle.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/eid.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct bundle *agent_create_bundle(const uint8_t bp_version,
	const char *local_eid, char *sink_id, char *destination,
	const uint64_t creation_timestamp_ms, const uint64_t sequence_number,
	const uint64_t lifetime_ms, void *payload, size_t payload_length,
	enum bundle_proc_flags flags)
{
	const size_t local_eid_length = strlen(local_eid);
	const size_t sink_length = strlen(sink_id);
	char *source_eid = malloc(local_eid_length + sink_length + 2);

	if (source_eid == NULL) {
		free(payload);
		return NULL;
	}

	memcpy(source_eid, local_eid, local_eid_length + 1); // include '\0'
	if (get_eid_scheme(source_eid) == EID_SCHEME_IPN) {
		char *const dot = strchr(source_eid, '.');

		if (!dot) {
			free(source_eid);
			free(payload);
			return NULL;
		}
		memcpy(&dot[1], sink_id, sink_length + 1);
	} else {
		memcpy(&source_eid[local_eid_length],
		       sink_id, sink_length + 1);
	}

	struct bundle *result;

	if (bp_version == 6)
		result = bundle6_create_local(
			payload, payload_length, source_eid, destination,
			creation_timestamp_ms, sequence_number,
			lifetime_ms, flags);
	else
		result = bundle7_create_local(
			payload, payload_length, source_eid, destination,
			creation_timestamp_ms, sequence_number,
			lifetime_ms, flags);

	free(source_eid);

	return result;
}

struct bundle *agent_create_forward_bundle(
	const struct bundle_agent_interface *bundle_agent_interface,
	const uint8_t bp_version, char *sink_id, char *destination,
	const uint64_t creation_timestamp_ms, const uint64_t sequence_number,
	const uint64_t lifetime_ms, void *payload, size_t payload_length,
	enum bundle_proc_flags flags)
{
	struct bundle *bundle = agent_create_bundle(
		bp_version,
		bundle_agent_interface->local_eid,
		sink_id,
		destination,
		creation_timestamp_ms,
		sequence_number,
		lifetime_ms,
		payload,
		payload_length,
		flags
	);

	if (!bundle)
		return NULL;

	bundle_processor_inform(
		bundle_agent_interface->bundle_signaling_queue,
		(struct bundle_processor_signal){
			.type = BP_SIGNAL_BUNDLE_LOCAL_DISPATCH,
			.bundle = bundle,
		}
	);

	return bundle;
}

struct bundle *agent_create_forward_bundle_direct(
	const struct bp_context *bundle_processor_context,
	const char *local_eid,
	const uint8_t bp_version, char *sink_id, char *destination,
	const uint64_t creation_timestamp_s, const uint64_t sequence_number,
	const uint64_t lifetime, void *payload, size_t payload_length,
	enum bundle_proc_flags flags)
{
	struct bundle *bundle = agent_create_bundle(
		bp_version,
		local_eid,
		sink_id,
		destination,
		creation_timestamp_s,
		sequence_number,
		lifetime,
		payload,
		payload_length,
		flags
	);

	if (!bundle)
		return NULL;

	const enum ud3tn_result dispatch_res = bundle_processor_bundle_dispatch(
		(void *)bundle_processor_context,
		bundle
	);

	return dispatch_res == UD3TN_OK ? bundle : NULL;
}
