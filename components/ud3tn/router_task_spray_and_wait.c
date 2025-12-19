// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

#include "ud3tn/bundle.h"
#include "ud3tn/node.h"
#include "ud3tn/result.h"
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
	const uint64_t timestamp_ms = hal_time_get_timestamp_ms();
	if (bundle_get_expiration_time_ms(b) < timestamp_ms) {
		return ROUTER_RESULT_EXPIRED;
	}

	struct node_list* node_list = routing_table_get_node_list();

	enum router_result_status status = ROUTER_RESULT_NO_ROUTE;

	while (node_list != NULL) {
		struct node* node = node_list->node;

		struct contact_list* contact_list = node->contacts;
		while(contact_list != NULL){
			if(contact_list->data->active){
				if(router_add_bundle_to_contact(contact_list->data, b) == UD3TN_FAIL) {
					LOGF_ERROR("Failed to emit bundle %p to %s", b, node->eid);
				} else {
					status = ROUTER_RESULT_OK;
				}
				break;
			}
			contact_list = contact_list->next;
		}
		node_list = node_list->next;
	}

	return status;
}

#endif