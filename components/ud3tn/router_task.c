// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/bundle.h"
#include "ud3tn/bundle_fragmenter.h"
#include "ud3tn/common.h"
#include "ud3tn/config.h"
#include "ud3tn/node.h"
#include "ud3tn/router.h"
#include "ud3tn/routing_table.h"

#include "platform/hal_io.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// COMMAND HANDLING

static bool process_router_command(
	struct router_command *router_cmd,
	struct rescheduling_handle rescheduler)
{
	switch (router_cmd->type) {
	case ROUTER_COMMAND_ADD:
		return routing_table_add_node(
			router_cmd->data,
			rescheduler
		);
	case ROUTER_COMMAND_UPDATE:
		return routing_table_replace_node(
			router_cmd->data,
			rescheduler
		);
	case ROUTER_COMMAND_DELETE:
		return routing_table_delete_node(
			router_cmd->data,
			rescheduler
		);
	default:
		free_node(router_cmd->data);
		return false;
	}
}

enum ud3tn_result router_process_command(
	struct router_command *command,
	struct rescheduling_handle rescheduler)
{
	bool success = true;

	success = process_router_command(
		command,
		rescheduler
	);
	if (success) {
		LOGF("Router: Command (T = %c) processed.",
			command->type);
	} else {
		LOGF("Router: Processing command (T = %c) failed!",
			command->type);
	}
	free(command);

	return success ? UD3TN_OK : UD3TN_FAIL;
}

// BUNDLE HANDLING

struct bundle_processing_result {
	int32_t status_or_fragments;
	struct bundle *fragments[ROUTER_MAX_FRAGMENTS];
};

#define BUNDLE_RESULT_NO_ROUTE 0
#define BUNDLE_RESULT_NO_TIMELY_CONTACTS -1
#define BUNDLE_RESULT_NO_MEMORY -2
#define BUNDLE_RESULT_INVALID -3
#define BUNDLE_RESULT_EXPIRED -4

static inline enum router_result_status br_to_rrs(int8_t bh_result)
{
	switch (bh_result) {
	case BUNDLE_RESULT_NO_ROUTE:
		return ROUTER_RESULT_NO_ROUTE;
	case BUNDLE_RESULT_NO_MEMORY:
		return ROUTER_RESULT_NO_MEMORY;
	case BUNDLE_RESULT_EXPIRED:
		return ROUTER_RESULT_EXPIRED;
	case BUNDLE_RESULT_NO_TIMELY_CONTACTS:
	default:
		return ROUTER_RESULT_NO_TIMELY_CONTACTS;
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
	const uint64_t timestamp_ms = hal_time_get_timestamp_ms();

	if (bundle_get_expiration_time_ms(bundle) < timestamp_ms) {
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
			LOGF("Router: Scheduling bundle %p failed, dropping all fragments.",
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

enum router_result_status router_route_bundle(struct bundle *b)
{
	struct bundle_processing_result proc_result = {
		.status_or_fragments = BUNDLE_RESULT_INVALID
	};

	if (b != NULL)
		proc_result = process_bundle(b);

	// b should not be used anymore, may be dropped due to
	// fragmentation.
	struct bundle *const b_old_ptr = b;

	b = NULL;
	if (IS_DEBUG_BUILD)
		LOGF(
			"Router: Bundle %p [ %s ] [ frag = %d ]",
			b_old_ptr,
			(proc_result.status_or_fragments < 1)
				? "ERR" : "OK",
			proc_result.status_or_fragments
		);
	if (proc_result.status_or_fragments < 1)
		return br_to_rrs(proc_result.status_or_fragments);
	return ROUTER_RESULT_OK;
}
