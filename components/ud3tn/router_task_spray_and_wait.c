// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

#include "ud3tn/bundle.h"
#include "ud3tn/bundle_fragmenter.h"
#include "ud3tn/common.h"
#include "ud3tn/node.h"
#include "ud3tn/router.h"
#include "ud3tn/routing_table.h"

#include "platform/hal_io.h"
#include "platform/hal_time.h"

#include <stdbool.h>
#include <stdlib.h>

#ifdef ROUTING_SPRAY_AND_WAIT

// BUNDLE HANDLING

enum router_result_status router_route_bundle(struct bundle *b)
{
	LOG_ERROR("router_route_bundle not yet implemented");
	return ROUTER_RESULT_OK;
}

#endif