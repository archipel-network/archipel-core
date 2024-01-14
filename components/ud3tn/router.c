// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/bundle.h"
#include "ud3tn/common.h"
#include "ud3tn/eid.h"
#include "ud3tn/node.h"
#include "ud3tn/router.h"
#include "ud3tn/routing_table.h"

#include "cla/cla.h"

#include "platform/hal_io.h"
#include "platform/hal_time.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static struct router_config RC = {
	.global_mbs = ROUTER_GLOBAL_MBS,
	.fragment_min_payload = FRAGMENT_MIN_PAYLOAD,
	.router_min_contacts_htab = ROUTER_MIN_CONTACTS_HTAB,
};

struct router_config router_get_config(void)
{
	return RC;
}

void router_update_config(struct router_config conf)
{
	RC = conf;
}

struct contact_list *router_lookup_destination(char *const dest)
{
	char *dest_node_eid = get_node_id(dest);
	const struct node_table_entry *e = NULL;

	if (dest_node_eid)
		e = routing_table_lookup_eid(dest_node_eid);

	// Fallback: perform a "dumb" string lookup
	if (!dest_node_eid || !e)
		e = routing_table_lookup_eid(dest);

	struct contact_list *result = NULL;

	if (e != NULL) {
		struct contact_list *cur = e->contacts;

		while (cur != NULL) {
			add_contact_to_ordered_list(&result, cur->data, 0);
			cur = cur->next;
		}
	}

	free(dest_node_eid);

	return result;
}

static inline struct max_fragment_size_result {
	uint32_t max_fragment_size;
	uint32_t payload_capacity;
} router_get_max_reasonable_fragment_size(
	struct contact_list *contacts, uint32_t full_size,
	uint32_t max_fragment_min_size, uint32_t payload_size,
	enum bundle_routing_priority priority, uint64_t exp_time)
{
	uint32_t payload_capacity = 0;
	uint32_t max_frag_size = UINT32_MAX;
	uint32_t min_capacity, c_capacity;
	int32_t c_pay_capacity;
	struct contact *c;

	(void)exp_time;
	min_capacity = payload_size / ROUTER_MAX_FRAGMENTS;
	min_capacity += max_fragment_min_size;
	while (contacts != NULL && payload_capacity < payload_size) {
		c = contacts->data;
		contacts = contacts->next;
		//if (c->to_s > exp_time)
		//	break;
		c_capacity = ROUTER_CONTACT_CAPACITY(c, priority);
		if (c_capacity < min_capacity)
			continue;

		// A CLA may have a maximum bundle size, determine it
		struct cla_config *const cla_config = cla_config_get(
			c->node->cla_addr
		);

		if (!cla_config)
			continue;

		const size_t c_mbs = MIN(
			MIN(
				(size_t)c_capacity,
				cla_config->vtable->cla_mbs_get(cla_config)
			),
			RC.global_mbs
		);

		// Contact of "infinite" capacity -> max. frag. size == MBS
		if (c_capacity >= INT32_MAX) {
			const uint32_t max_fragment_size = MIN(
				(uint32_t)INT32_MAX,
				c_mbs
			);

			return (struct max_fragment_size_result){
				max_fragment_size,
				payload_size,
			};
		}

		c_pay_capacity = c_capacity - max_fragment_min_size;
		if (c_pay_capacity > RC.fragment_min_payload) {
			payload_capacity += c_pay_capacity;
			max_frag_size = MIN(max_frag_size, c_mbs);
			if (c_capacity >= full_size)
				break;
		}
	}
	return (struct max_fragment_size_result){
		payload_capacity < payload_size ? 0 : max_frag_size,
		payload_capacity,
	};
}

uint8_t router_calculate_fragment_route(
	struct fragment_route *res, uint32_t size,
	struct contact_list *contacts, uint32_t preprocessed_size,
	enum bundle_routing_priority priority, uint64_t exp_time_ms,
	struct contact **excluded_contacts, uint8_t excluded_contacts_count)
{
	const uint64_t time_ms = hal_time_get_timestamp_ms();
	uint32_t cap;
	uint8_t d, i;
	struct contact *c;

	res->contact = NULL;
	res->preemption_improved = 0;
	while (contacts != NULL) {
		c = contacts->data;
		contacts = contacts->next;
		d = 0;
		for (i = 0; i < excluded_contacts_count; i++)
			if (c == excluded_contacts[i])
				d = 1;
		if (d)
			continue;
		if (c->from_ms >= exp_time_ms)
			continue; // Ignore -- NOTE: List is ordered by c->to
		if (c->to_ms <= time_ms)
			continue;
		cap = ROUTER_CONTACT_CAPACITY(c, 0);
		if (preprocessed_size != 0) {
			if (preprocessed_size >= cap) {
				preprocessed_size -= cap;
				continue;
			} else {
				cap -= preprocessed_size;
				/* Don't set to zero here, needed in next if */
			}
		}
		if (cap < size) {
			if ((ROUTER_CONTACT_CAPACITY(c, priority)
					- preprocessed_size) >= size)
				res->preemption_improved++;
			preprocessed_size = 0;
			continue;
		}
		res->contact = c;
		break;
	}
	return (res->contact != NULL);
}

static inline void router_get_first_route_nonfrag(
	struct router_result *res, struct contact_list *contacts,
	struct bundle *bundle, uint32_t bundle_size,
	uint64_t expiration_time_ms)
{
	res->fragment_results[0].payload_size = bundle->payload_block->length;
	/* Determine route */
	if (router_calculate_fragment_route(
		&res->fragment_results[0], bundle_size,
		contacts, 0, ROUTER_BUNDLE_PRIORITY(bundle), expiration_time_ms,
		NULL, 0)
	) {
		res->fragments = 1;
		res->preemption_improved = res->fragment_results[0].preemption_improved;
	}
}

static inline void router_get_first_route_frag(
	struct router_result *res, struct contact_list *contacts,
	struct bundle *bundle, uint32_t bundle_size,
	uint64_t expiration_time_ms,
	uint32_t max_frag_sz, uint32_t first_frag_sz, uint32_t last_frag_sz)
{
	uint32_t mid_frag_sz, next_frag_sz, remaining_pay, processed_sz;
	int32_t min_pay, max_pay;
	int32_t success, index;

	/* Determine fragment minimum sizes */
	mid_frag_sz = bundle_get_mid_fragment_min_size(bundle);
	next_frag_sz = first_frag_sz;
	if (next_frag_sz > max_frag_sz || last_frag_sz > max_frag_sz) {
		LOGF_INFO(
			"Router: Cannot fragment because max. frag. size of %lu bytes is smaller than bundle headers (first = %lu, mid = %lu, last = %lu)",
			max_frag_sz,
			next_frag_sz,
			mid_frag_sz,
			last_frag_sz
		);
		return; /* failed */
	}

	remaining_pay = bundle->payload_block->length;
	while (remaining_pay != 0 && res->fragments < ROUTER_MAX_FRAGMENTS) {
		min_pay = MIN(remaining_pay, RC.fragment_min_payload);
		max_pay = max_frag_sz - next_frag_sz;
		if (max_pay < min_pay) {
			LOGF_INFO(
				"Router: Cannot fragment because minimum amount of payload (%lu bytes) will not fit in fragment with maximum payload size of %lu bytes",
				min_pay,
				max_pay
			);
			break; /* failed: remaining_pay != 0 */
		}
		if (remaining_pay <= max_frag_sz - last_frag_sz) {
			/* Last fragment */
			res->fragment_results[res->fragments++].payload_size = remaining_pay;
			remaining_pay = 0;
		} else {
			/* Another fragment */
			max_pay = MIN((int32_t)remaining_pay, max_pay);
			res->fragment_results[res->fragments++].payload_size = max_pay;
			remaining_pay -= max_pay;
			next_frag_sz = mid_frag_sz;
		}
	}
	if (remaining_pay != 0) {
		res->fragments = 0;
		return; /* failed */
	}

	/* Determine routes */
	success = 0;
	res->preemption_improved = 0;
	processed_sz = 0;
	for (index = 0; index < res->fragments; index++) {
		bundle_size = res->fragment_results[index].payload_size;
		if (index == 0)
			bundle_size += first_frag_sz;
		else if (index == res->fragments - 1)
			bundle_size += last_frag_sz;
		else
			bundle_size += mid_frag_sz;
		success += router_calculate_fragment_route(
			&res->fragment_results[index], bundle_size,
			contacts, processed_sz, ROUTER_BUNDLE_PRIORITY(bundle),
			expiration_time_ms, NULL, 0);
		res->preemption_improved +=
			res->fragment_results[index].preemption_improved;
		processed_sz += bundle_size;
	}
	if (success != res->fragments)
		res->fragments = 0;
}

/* max. ~200 bytes on stack */
struct router_result router_get_first_route(struct bundle *bundle)
{
	const uint64_t expiration_time_ms = bundle_get_expiration_time_ms(
		bundle
	);
	struct router_result res;
	struct contact_list *contacts =
		router_lookup_destination(bundle->destination);

	res.fragments = 0;
	res.preemption_improved = 0;
	if (contacts == NULL) {
		LOGF_INFO(
			"Router: Could not determine a node over which the destination \"%s\" for bundle %p is reachable",
			bundle->destination,
			bundle
		);
		return res;
	}

	const uint32_t bundle_size = bundle_get_serialized_size(bundle);
	const uint32_t first_frag_sz = bundle_get_first_fragment_min_size(
		bundle
	);
	const uint32_t last_frag_sz = bundle_get_last_fragment_min_size(bundle);

	const struct max_fragment_size_result mrfs =
		router_get_max_reasonable_fragment_size(
			contacts,
			bundle_size,
			MAX(first_frag_sz, last_frag_sz),
			bundle->payload_block->length,
			ROUTER_BUNDLE_PRIORITY(bundle),
			expiration_time_ms
		);

	if (mrfs.max_fragment_size == 0) {
		LOGF_DEBUG(
			"Router: Contact payload capacity (%lu bytes) too low for bundle %p of size %lu bytes (min. frag. sz. = %lu, payload sz. = %lu)",
			mrfs.payload_capacity,
			bundle,
			bundle_size,
			MAX(first_frag_sz, last_frag_sz),
			bundle->payload_block->length
		);
		goto finish;
	} else if (mrfs.max_fragment_size != INT32_MAX) {
		LOGF_DEBUG(
			"Router: Determined max. frag size of %lu bytes for bundle %p of size %lu bytes (payload sz. = %lu)",
			mrfs.max_fragment_size,
			bundle,
			bundle_size,
			bundle->payload_block->length
		);
	} else {
		LOGF_DEBUG(
			"Router: Determined infinite max. frag size for bundle of size %lu bytes (payload sz. = %lu)",
			bundle_size,
			bundle->payload_block->length
		);
	}

	if (bundle_must_not_fragment(bundle) ||
			bundle_size <= mrfs.max_fragment_size)
		router_get_first_route_nonfrag(&res,
			contacts, bundle, bundle_size, expiration_time_ms);
	else
		router_get_first_route_frag(&res,
			contacts, bundle, bundle_size, expiration_time_ms,
			mrfs.max_fragment_size, first_frag_sz, last_frag_sz);

	if (!res.fragments)
		LOGF_INFO(
			"Router: No feasible route found for bundle %p to \"%s\" with size of %lu bytes",
			bundle,
			bundle->destination,
			bundle_size
		);

finish:
	while (contacts) {
		struct contact_list *const tmp = contacts->next;

		free(contacts);
		contacts = tmp;
	}
	return res;
}

/* For use with caching of routes */
struct router_result router_try_reuse(
	struct router_result route, struct bundle *bundle)
{
	const uint64_t time_ms = hal_time_get_timestamp_ms();
	const uint64_t expiration_time_ms = bundle_get_expiration_time_ms(
		bundle
	);
	uint32_t remaining_pay = bundle->payload_block->length;
	uint32_t size, min_cap;
	struct fragment_route *fr;
	int32_t f;

	if (route.fragments == 0)
		return route;

	/* Not fragmented */
	if (bundle_must_not_fragment(bundle) || route.fragments == 1) {
		size = bundle_get_serialized_size(bundle);
		fr = &route.fragment_results[0];
		fr->payload_size = remaining_pay;
		if (fr->contact->to_ms <= time_ms ||
		    fr->contact->to_ms > expiration_time_ms ||
		    ROUTER_CONTACT_CAPACITY(fr->contact, 0) < (int32_t)size)
			route.fragments = 0;
		return route;
	}

	/* Fragmented */
	for (f = 0; f < route.fragments; f++) {
		if (f == 0)
			size = bundle_get_first_fragment_min_size(bundle);
		else if (f == route.fragments - 1)
			size = bundle_get_last_fragment_min_size(bundle);
		else
			size = bundle_get_mid_fragment_min_size(bundle);
		fr = &route.fragment_results[f];
		min_cap = UINT32_MAX;
		if (fr->contact->to_ms <= time_ms ||
		    fr->contact->to_ms > expiration_time_ms ||
		    (ROUTER_CONTACT_CAPACITY(fr->contact, 0) <
		     (int32_t)(size + RC.fragment_min_payload))) {
			route.fragments = 0;
			return route;
		}
		min_cap = MIN(min_cap,
			ROUTER_CONTACT_CAPACITY(fr->contact, 0)
			- size);
		fr->payload_size = MIN(remaining_pay, min_cap);
		remaining_pay -= fr->payload_size;
		if (remaining_pay == 0) {
			route.fragments = f - 1;
			return route;
		}
	}

	if (remaining_pay != 0)
		route.fragments = 0;
	return route;
}

enum ud3tn_result router_add_bundle_to_contact(
	struct contact *contact, struct bundle *b)
{
	struct routed_bundle_list *new_entry, **cur_entry;

	ASSERT(contact != NULL);
	ASSERT(b != NULL);
	if (!contact || !b)
		return UD3TN_FAIL;
	ASSERT(contact->remaining_capacity_p0 > 0);

	new_entry = malloc(sizeof(struct routed_bundle_list));
	if (new_entry == NULL)
		return UD3TN_FAIL;
	new_entry->data = b;
	new_entry->next = NULL;
	cur_entry = &contact->contact_bundles;
	/* Go to end of list (=> FIFO) */
	while (*cur_entry != NULL) {
		ASSERT((*cur_entry)->data != b);
		if ((*cur_entry)->data == b) {
			free(new_entry);
			return UD3TN_FAIL;
		}
		cur_entry = &(*cur_entry)->next;
	}
	*cur_entry = new_entry;
	// This contact is of infinite capacity, just return "OK".
	if (contact->remaining_capacity_p0 == INT32_MAX)
		return UD3TN_OK;

	const size_t bundle_size = bundle_get_serialized_size(b);
	const enum bundle_routing_priority prio =
		bundle_get_routing_priority(b);

	contact->remaining_capacity_p0 -= bundle_size;
	if (prio > BUNDLE_RPRIO_LOW) {
		contact->remaining_capacity_p1 -= bundle_size;
		if (prio != BUNDLE_RPRIO_NORMAL)
			contact->remaining_capacity_p2 -= bundle_size;
	}
	return UD3TN_OK;
}

enum ud3tn_result router_remove_bundle_from_contact(
	struct contact *contact, struct bundle *bundle)
{
	struct routed_bundle_list **cur_entry, *tmp;

	ASSERT(contact != NULL);
	if (!contact)
		return UD3TN_FAIL;
	cur_entry = &contact->contact_bundles;
	/* Find bundle */
	while (*cur_entry != NULL) {
		ASSERT((*cur_entry)->data != NULL);
		if ((*cur_entry)->data == bundle) {
			tmp = *cur_entry;
			*cur_entry = (*cur_entry)->next;
			free(tmp);
			// This contact is of infinite capacity, do nothing.
			if (contact->remaining_capacity_p0 == INT32_MAX)
				continue;

			const size_t bundle_size =
				bundle_get_serialized_size(bundle);
			const enum bundle_routing_priority prio =
				bundle_get_routing_priority(bundle);

			contact->remaining_capacity_p0 += bundle_size;
			if (prio > BUNDLE_RPRIO_LOW) {
				contact->remaining_capacity_p1 += bundle_size;
				if (prio != BUNDLE_RPRIO_NORMAL)
					contact->remaining_capacity_p2 += bundle_size;
			}
			return UD3TN_OK;
		}
		cur_entry = &(*cur_entry)->next;
	}
	return UD3TN_FAIL;
}
