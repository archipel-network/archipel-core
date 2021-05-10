#include "ud3tn/bundle.h"
#include "ud3tn/common.h"
#include "ud3tn/node.h"
#include "ud3tn/router.h"
#include "ud3tn/routing_table.h"

#include "cla/cla.h"

#include "platform/hal_io.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static struct router_config RC = {
	.global_mbs = ROUTER_GLOBAL_MBS,
	.fragment_min_payload = FRAGMENT_MIN_PAYLOAD,
	.min_probability = MIN_PROBABILITY,
	.min_node_confidence_opportunistic = MIN_NODE_CONFIDENCE_OPPORTUNISTIC,
	.min_node_confidence_deterministic = MIN_NODE_CONFIDENCE_DETERMINISTIC,
	.node_trustworthiness_weight = NODE_TRUSTWORTHINESS_WEIGHT,
	.node_reliability_weight = NODE_RELIABILITY_WEIGHT,
	.opt_min_time = OPTIMIZATION_MIN_TIME,
	.opt_max_bundles = OPTIMIZATION_MAX_BUNDLES,
	.opt_max_pre_bundles = OPTIMIZATION_MAX_PRE_BUNDLES,
	.opt_max_pre_bundles_contact = OPTIMIZATION_MAX_PRE_BUNDLES_CONTACT,
	.router_min_contacts_htab = ROUTER_MIN_CONTACTS_HTAB,
	.router_def_base_reliability = ROUTER_DEF_BASE_RELIABILITY
};

void router_optimizer_update_config_int(struct router_config conf);

struct router_config router_get_config(void)
{
	return RC;
}

enum ud3tn_result router_update_config(struct router_config conf)
{
	float w = (
		conf.node_trustworthiness_weight +
		conf.node_reliability_weight
	);

	if (conf.min_probability > 1.0f
		|| conf.min_probability <= 0
		|| conf.min_node_confidence_opportunistic > 1.0f
		|| conf.min_node_confidence_deterministic > 1.0f
		|| conf.min_node_confidence_opportunistic <= 0
		|| conf.min_node_confidence_deterministic <= 0
		|| conf.min_node_confidence_opportunistic
			> conf.min_node_confidence_deterministic
		|| conf.min_node_confidence_deterministic
			< conf.min_probability
		|| w > 1.0001f || w < 0.9999f
		|| conf.opt_max_bundles < 1
		|| conf.opt_max_pre_bundles_contact > conf.opt_max_pre_bundles
		|| conf.router_def_base_reliability > 1.0f
		|| conf.router_def_base_reliability <= 0
	) {
		return UD3TN_FAIL;
	}
	RC = conf;
	router_optimizer_update_config_int(conf);
	return UD3TN_OK;
}

struct associated_contact_list *router_lookup_destination(char *const dest)
{
	const char *const DTN_SCHEME = "dtn://";
	const size_t DTN_SCHEME_LENGTH = strlen(DTN_SCHEME);

	char *dest_node_eid = dest;
	bool free_dest_node_eid = false;
	char *const found = strstr(dest, DTN_SCHEME);

	// We only support dtn://node_id/app_id decoding for dtn:// EIDs
	if (found == dest) {
		char *const node_id_end = strchr(
			dest + DTN_SCHEME_LENGTH, '/'
		);

		if (node_id_end) {
			const size_t node_id_len = node_id_end - dest;
			char *const node_id = malloc(node_id_len + 1);

			strncpy(node_id, dest, node_id_len);
			node_id[node_id_len] = 0;
			dest_node_eid = node_id;
			free_dest_node_eid = true;
		}
	}

	const struct node_table_entry *const e = routing_table_lookup_eid(
		dest_node_eid
	);
	uint16_t results = 0;
	struct associated_contact_list *result = NULL;

	if (e != NULL) {
		struct associated_contact_list *cur = e->contacts;

		while (cur != NULL) {
			add_contact_to_ordered_assoc_list(
				&result, cur->data, cur->p, 0);
			results++;
			cur = cur->next;
		}
	}

	if (free_dest_node_eid)
		free(dest_node_eid);

	return result;
}

static inline struct max_fragment_size_result {
	uint32_t max_fragment_size;
	uint32_t payload_capacity;
} router_get_max_reasonable_fragment_size(
	struct associated_contact_list *contacts, uint32_t full_size,
	uint32_t max_fragment_min_size, uint32_t payload_size,
	enum bundle_routing_priority priority, uint64_t exp_time)
{
	uint32_t payload_capacity = 0;
	uint32_t max_frag_size = UINT32_MAX;
	uint32_t min_capacity, c_capacity;
	int32_t c_pay_capacity;
	struct contact *c;
	float conf, p;

	(void)exp_time;
	min_capacity = payload_size / ROUTER_MAX_FRAGMENTS;
	min_capacity += max_fragment_min_size;
	while (contacts != NULL && payload_capacity < payload_size) {
		c = contacts->data;
		p = contacts->p;
		contacts = contacts->next;
		//if (c->to > exp_time)
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

		conf = ROUTER_CONTACT_CONFIDENCE(c, p);
		if (conf >= RC.min_node_confidence_deterministic) {
			c_pay_capacity = c_capacity - max_fragment_min_size;
			if (c_pay_capacity > RC.fragment_min_payload) {
				payload_capacity += c_pay_capacity;
				max_frag_size = MIN(max_frag_size, c_mbs);
				if (c_capacity >= full_size)
					break;
			}
		} else if (conf >= RC.min_node_confidence_opportunistic) {
			c_pay_capacity = c_capacity - max_fragment_min_size;
			if (c_pay_capacity > RC.fragment_min_payload) {
				/* Reasonable? */
				payload_capacity
					+= (uint32_t)(conf * c_pay_capacity);
				max_frag_size = MIN(max_frag_size, c_mbs);
			}
		}
	}
	return (struct max_fragment_size_result){
		payload_capacity < payload_size ? 0 : max_frag_size,
		payload_capacity,
	};
}

uint8_t router_calculate_fragment_route(
	struct fragment_route *res, uint32_t size,
	struct associated_contact_list *contacts, uint32_t preprocessed_size,
	enum bundle_routing_priority priority, uint64_t exp_time,
	struct contact **excluded_contacts, uint8_t excluded_contacts_count)
{
	uint64_t time = hal_time_get_timestamp_s();
	uint32_t cap;
	uint8_t d, i;
	float conf, p;
	struct contact *c;

	(void)exp_time;
	res->contact_count = 0;
	res->probability = 0;
	res->preemption_improved = 0;
	while (contacts != NULL
		&& res->probability < RC.min_probability
		&& res->contact_count != ROUTER_MAX_CONTACTS
	) {
		c = contacts->data;
		p = contacts->p;
		contacts = contacts->next;
		d = 0;
		for (i = 0; i < excluded_contacts_count; i++)
			if (c == excluded_contacts[i])
				d = 1;
		if (d)
			continue;
		if (c->from >= exp_time)
			continue; // Ignore -- NOTE: List is ordered by c->to
		if (c->to <= time)
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
		} else {
			preprocessed_size = 0;
		}
		conf = ROUTER_CONTACT_CONFIDENCE(c, p);
		if (conf >= RC.min_node_confidence_deterministic) {
			/* Deterministic routing */
			res->contact_count = 0;
			res->contacts[res->contact_count++] = c;
			res->probability = conf;
			break;
		} else if (conf >= RC.min_node_confidence_opportunistic) {
			/* Opportunistic routing */
			res->contacts[res->contact_count++] = c;
			/* P(A u B) = P(A) + P(B) - P(A n B) */
			res->probability = res->probability + conf
				- (res->probability * conf);
		}
	}
	return (res->probability >= RC.min_probability);
}

static inline void router_get_first_route_nonfrag(
	struct router_result *res, struct associated_contact_list *contacts,
	struct bundle *bundle, uint32_t bundle_size, uint64_t expiration_time)
{
	res->fragment_results[0].payload_size
		= bundle->payload_block->length;
	/* Determine route */
	if (router_calculate_fragment_route(
		&res->fragment_results[0], bundle_size,
		contacts, 0, ROUTER_BUNDLE_PRIORITY(bundle), expiration_time,
		NULL, 0)
	) {
		res->fragments = 1;
		res->probability
			= res->fragment_results[0].probability;
		res->preemption_improved
			= res->fragment_results[0].preemption_improved;
	}
}

static inline void router_get_first_route_frag(
	struct router_result *res, struct associated_contact_list *contacts,
	struct bundle *bundle, uint32_t bundle_size, uint64_t expiration_time,
	uint32_t max_frag_sz, uint32_t first_frag_sz, uint32_t last_frag_sz)
{
	uint32_t mid_frag_sz, next_frag_sz, remaining_pay, processed_sz;
	int32_t min_pay, max_pay;
	int32_t success, index;

	/* Determine fragment minimum sizes */
	mid_frag_sz = bundle_get_mid_fragment_min_size(bundle);
	next_frag_sz = first_frag_sz;
	if (next_frag_sz > max_frag_sz || last_frag_sz > max_frag_sz) {
		LOGF("Router: Cannot fragment because max. frag. size of %lu bytes is smaller than bundle headers (first = %lu, mid = %lu, last = %lu)",
		     max_frag_sz, next_frag_sz, mid_frag_sz, last_frag_sz);
		return; /* failed */
	}

	remaining_pay = bundle->payload_block->length;
	while (remaining_pay != 0 && res->fragments < ROUTER_MAX_FRAGMENTS) {
		min_pay = MIN(remaining_pay, RC.fragment_min_payload);
		max_pay = max_frag_sz - next_frag_sz;
		if (max_pay < min_pay) {
			LOGF("Router: Cannot fragment because minimum amount of payload (%lu bytes) will not fit in fragment with maximum payload size of %lu bytes",
			     min_pay, max_pay);
			break; /* failed: remaining_pay != 0 */
		}
		if (remaining_pay <= max_frag_sz - last_frag_sz) {
			/* Last fragment */
			res->fragment_results[res->fragments++].payload_size
				= remaining_pay;
			remaining_pay = 0;
		} else {
			/* Another fragment */
			max_pay = MIN((int32_t)remaining_pay, max_pay);
			res->fragment_results[res->fragments++].payload_size
				= max_pay;
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
	res->probability = 1;
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
			expiration_time, NULL, 0);
		res->probability *= res->fragment_results[index].probability;
		res->preemption_improved
			+= res->fragment_results[index].preemption_improved;
		processed_sz += bundle_size;
	}
	if (success != res->fragments) {
		res->fragments = 0;
		res->probability = 0;
	}
}

/* max. ~200 bytes on stack */
struct router_result router_get_first_route(struct bundle *bundle)
{
	const uint64_t expiration_time = bundle_get_expiration_time_s(bundle);
	struct router_result res;
	struct associated_contact_list *contacts
		= router_lookup_destination(bundle->destination);

	res.fragments = 0;
	res.probability = 0.0f;
	if (contacts == NULL) {
		LOGF("Router: Could not determine a node over which the destination \"%s\" is reachable",
		     bundle->destination);
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
			expiration_time
		);

	if (mrfs.max_fragment_size == 0) {
		LOGF("Router: Contact payload capacity (%lu bytes) too low for bundle of size %lu bytes (min. frag. sz. = %lu, payload sz. = %lu)",
		     mrfs.payload_capacity, bundle_size,
		     MAX(first_frag_sz, last_frag_sz),
		     bundle->payload_block->length);
		goto finish;
	} else if (mrfs.max_fragment_size != UINT32_MAX) {
		LOGF("Router: Determined max. frag size of %lu bytes for bundle of size %lu bytes (payload sz. = %lu)",
		     mrfs.max_fragment_size, bundle_size,
		     bundle->payload_block->length);
	}

	if (bundle_must_not_fragment(bundle) ||
			bundle_size <= mrfs.max_fragment_size)
		router_get_first_route_nonfrag(&res,
			contacts, bundle, bundle_size, expiration_time);
	else
		router_get_first_route_frag(&res,
			contacts, bundle, bundle_size, expiration_time,
			mrfs.max_fragment_size, first_frag_sz, last_frag_sz);

	if (!res.fragments)
		LOGF("Router: No feasible route found for bundle to \"%s\" with size of %lu bytes",
		     bundle->destination, bundle_size);

finish:
	list_free(contacts);
	return res;
}

/* For use with caching of routes */
struct router_result router_try_reuse(
	struct router_result route, struct bundle *bundle)
{
	uint64_t time = hal_time_get_timestamp_s();
	uint64_t expiration_time = bundle_get_expiration_time_s(bundle);
	uint32_t remaining_pay = bundle->payload_block->length;
	uint32_t size, min_cap;
	struct fragment_route *fr;
	uint8_t c;
	int32_t f;

	if (route.fragments == 0 || route.probability < RC.min_probability)
		return route;

	/* Not fragmented */
	if (bundle_must_not_fragment(bundle) || route.fragments == 1) {
		size = bundle_get_serialized_size(bundle);
		fr = &route.fragment_results[0];
		fr->payload_size = remaining_pay;
		for (c = 0; c < fr->contact_count; c++) {
			if (fr->contacts[c]->to <= time
				|| fr->contacts[c]->to > expiration_time
				|| ROUTER_CONTACT_CAPACITY(fr->contacts[c], 0)
					< (int32_t)size
			) {
				route.fragments = 0;
				route.probability = 0;
				return route;
			}
		}
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
		for (c = 0; c < fr->contact_count; c++) {
			if (fr->contacts[c]->to <= time
				|| fr->contacts[c]->to > expiration_time
				|| ROUTER_CONTACT_CAPACITY(fr->contacts[c], 0)
					< (int32_t)(size
						+ RC.fragment_min_payload)
			) {
				route.fragments = 0;
				route.probability = 0;
				return route;
			}
			min_cap = MIN(min_cap,
				ROUTER_CONTACT_CAPACITY(fr->contacts[c], 0)
				- size);

		}
		fr->payload_size = MIN(remaining_pay, min_cap);
		remaining_pay -= fr->payload_size;
		if (remaining_pay == 0) {
			route.fragments = f - 1;
			return route;
		}
	}

	if (remaining_pay != 0) {
		route.fragments = 0;
		route.probability = 0;
	}
	return route;
}

enum ud3tn_result router_add_bundle_to_contact(
	struct contact *contact, struct routed_bundle *rb)
{
	struct routed_bundle_list *new_entry, **cur_entry;

	ASSERT(contact != NULL);
	ASSERT(rb != NULL);
	ASSERT(contact->remaining_capacity_p0 > 0);
	new_entry = malloc(sizeof(struct routed_bundle_list));
	if (new_entry == NULL)
		return UD3TN_FAIL;
	new_entry->data = rb;
	new_entry->next = NULL;
	cur_entry = &contact->contact_bundles;
	/* Go to end of list (=> FIFO) */
	while (*cur_entry != NULL)
		cur_entry = &(*cur_entry)->next;
	*cur_entry = new_entry;
	contact->bundle_count++;
	contact->remaining_capacity_p0 -= rb->size;
	if (rb->prio > BUNDLE_RPRIO_LOW) {
		contact->remaining_capacity_p1 -= rb->size;
		if (rb->prio != BUNDLE_RPRIO_NORMAL)
			contact->remaining_capacity_p2 -= rb->size;
	}
	return UD3TN_OK;
}

struct routed_bundle *router_remove_bundle_from_contact(
	struct contact *contact, bundleid_t id)
{
	struct routed_bundle *rb;
	struct routed_bundle_list **cur_entry, *tmp;

	ASSERT(contact != NULL);
	cur_entry = &contact->contact_bundles;
	/* Find rb */
	while (*cur_entry != NULL) {
		ASSERT((*cur_entry)->data != NULL);
		if ((*cur_entry)->data->id == id) {
			rb = (*cur_entry)->data;
			tmp = *cur_entry;
			*cur_entry = (*cur_entry)->next;
			free(tmp);
			contact->bundle_count--;
			contact->remaining_capacity_p0 += rb->size;
			if (rb->prio > BUNDLE_RPRIO_LOW) {
				contact->remaining_capacity_p1 += rb->size;
				if (rb->prio != BUNDLE_RPRIO_NORMAL)
					contact->remaining_capacity_p2
						+= rb->size;
			}
			return rb;
		}
		cur_entry = &(*cur_entry)->next;
	}
	return NULL;
}

uint8_t router_add_bundle_to_route(struct fragment_route *r, struct bundle *b)
{
	uint8_t success;
	struct routed_bundle *rb;

	ASSERT(r != NULL);
	ASSERT(b != NULL);
	rb = malloc(sizeof(struct routed_bundle));
	if (rb == NULL)
		return 0;
	rb->id = b->id;
	rb->prio = bundle_get_routing_priority(b);
	rb->size = bundle_get_serialized_size(b);
	rb->exp_time = bundle_get_expiration_time_s(b);
	rb->destination = strdup(b->destination);
	if (rb->destination == NULL) {
		free(rb);
		return 0;
	}
	success = router_update_routed_bundle(r, rb);
	if (!success) {
		free(rb->destination);
		free(rb);
		return 0;
	}
	return 1;
}

uint8_t router_update_routed_bundle(
	struct fragment_route *r, struct routed_bundle *rb)
{
	uint8_t c, added_contacts;

	ASSERT(r != NULL);
	ASSERT(rb != NULL);
	ASSERT(r->contact_count != 0);
	rb->preemption_improvement = r->preemption_improved;
	rb->contact_count = r->contact_count;
	rb->contacts = malloc(sizeof(void *) * r->contact_count);
	if (rb->contacts == NULL)
		return 0;
	rb->transmitted = 0;
	rb->serialized = 0;
	added_contacts = 0;
	for (c = 0; c < r->contact_count; c++) {
		if (router_add_bundle_to_contact(r->contacts[c], rb)
				== UD3TN_OK)
			rb->contacts[added_contacts++] = r->contacts[c];
	}
	if (added_contacts != rb->contact_count) {
		if (added_contacts == 0) {
			free(rb->contacts);
			return 0;
		}
		rb->contact_count = added_contacts;
	}
	return 1;
}

void router_remove_bundle_from_route(
	struct fragment_route *r, bundleid_t id, uint8_t free_rb)
{
	uint8_t c;
	struct routed_bundle *rb = NULL, *tmp;

	ASSERT(r != NULL);
	for (c = 0; c < r->contact_count; c++) {
		tmp = router_remove_bundle_from_contact(
			r->contacts[c], id);
		if (tmp != NULL)
			rb = tmp;
	}
	if (free_rb && rb != NULL) {
		free(rb->destination);
		free(rb->contacts);
		free(rb);
	}
}
