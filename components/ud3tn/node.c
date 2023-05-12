// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/common.h"
#include "ud3tn/node.h"
#include "ud3tn/result.h"

#include "platform/hal_time.h"

#include "util/llsort.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int contacts_overlap(struct contact *a, struct contact *b)
{
	return (
		a->from_ms < b->to_ms &&
		a->to_ms > b->from_ms
	);
}

struct node *node_create(char *eid)
{
	struct node *ret = malloc(sizeof(struct node));

	if (ret == NULL)
		return NULL;

	ret->cla_addr = NULL;
	ret->flags = NODE_FLAG_NONE;
	ret->endpoints = NULL;
	ret->contacts = NULL;
	if (eid == NULL)
		ret->eid = NULL;
	else
		ret->eid = strdup(eid);
	return ret;
}

struct contact *contact_create(struct node *node)
{
	struct contact *ret = malloc(sizeof(struct contact));

	if (ret == NULL)
		return NULL;
	ret->node = node;
	ret->from_ms = 0;
	ret->to_ms = 0;
	ret->bitrate_bytes_per_s = 0;
	ret->total_capacity_bytes = 0;
	ret->remaining_capacity_p0 = 0;
	ret->remaining_capacity_p1 = 0;
	ret->remaining_capacity_p2 = 0;
	ret->contact_endpoints = NULL;
	ret->contact_bundles = NULL;
	ret->active = 0;
	return ret;
}

static void free_contact_internal(
	struct contact *contact, int free_eid_list)
{
	struct endpoint_list *cur_eid;
	struct routed_bundle_list *next, *cur_bundle;

	if (contact == NULL)
		return;
	ASSERT(contact->active == 0);
	if (free_eid_list) {
		cur_eid = contact->contact_endpoints;
		while (cur_eid != NULL)
			cur_eid = endpoint_list_free(cur_eid);
	}
	/* Free associated bundle list (not bundles themselves) */
	cur_bundle = contact->contact_bundles;
	while (cur_bundle != NULL) {
		next = cur_bundle->next;
		free(cur_bundle);
		cur_bundle = next;
	}
	free(contact);
}

void free_contact(struct contact *contact)
{
	free_contact_internal(contact, 1);
}

static struct contact_list *contact_list_free_internal(
	struct contact_list *e, int free_eid_list);

void free_node(struct node *node)
{
	struct endpoint_list *cur_eid;
	struct contact_list *cur_contact;

	if (node == NULL)
		return;
	cur_eid = node->endpoints;
	while (cur_eid != NULL)
		cur_eid = endpoint_list_free(cur_eid);
	cur_contact = node->contacts;
	while (cur_contact != NULL)
		cur_contact = contact_list_free_internal(cur_contact, 1);
	free(node->cla_addr);
	free(node->eid);
	free(node);
}

struct endpoint_list *endpoint_list_free(struct endpoint_list *e)
{
	struct endpoint_list *next;

	if (e == NULL)
		return NULL;
	next = e->next;
	free(e->eid);
	free(e);
	return next;
}

static enum ud3tn_result endpoint_list_add(
	struct endpoint_list **list, char *eid)
{
	struct endpoint_list **cur_entry, *new_entry;
	struct endpoint_list **target_pos = NULL;

	ASSERT(list != NULL);
	ASSERT(eid != NULL);
	if (!list || !eid)
		return UD3TN_FAIL;
	cur_entry = list;
	while (*cur_entry != NULL) {
		if ((*cur_entry)->eid == eid)
			return UD3TN_FAIL;
		if (strcmp((*cur_entry)->eid, eid) == 0) {
			// Already contained in list -> free the duplicate
			free(eid);
			return UD3TN_FAIL;
		}
		if ((*cur_entry)->eid > eid && !target_pos)
			target_pos = cur_entry;
		cur_entry = &(*cur_entry)->next;
	}
	if (!target_pos)
		target_pos = cur_entry;
	new_entry = malloc(sizeof(struct endpoint_list));
	if (new_entry == NULL) {
		free(eid);
		return UD3TN_FAIL;
	}
	new_entry->eid = eid;
	new_entry->next = *target_pos;
	*target_pos = new_entry;
	return UD3TN_OK;
}

static enum ud3tn_result endpoint_list_remove(
	struct endpoint_list **list, char *eid)
{
	struct endpoint_list **cur_entry, *tmp;

	ASSERT(list != NULL);
	ASSERT(eid != NULL);
	if (!list || !eid)
		return UD3TN_FAIL;
	cur_entry = list;
	while (*cur_entry != NULL) {
		if (strcmp((*cur_entry)->eid, eid) == 0) {
			tmp = *cur_entry;
			*cur_entry = (*cur_entry)->next;
			free(tmp);
			return UD3TN_OK;
		}
		cur_entry = &(*cur_entry)->next;
	}
	return UD3TN_FAIL;
}

int endpoint_list_sorted(struct endpoint_list *list)
{
	char *last = NULL;

	while (list != NULL) {
		if (list->eid < last)
			return 0;
		last = list->eid;
		list = list->next;
	}
	return 1;
}

struct endpoint_list *endpoint_list_union(
	struct endpoint_list *a, struct endpoint_list *b)
{
	struct endpoint_list *cur_can = b;

	while (cur_can) {
		endpoint_list_add(&a, cur_can->eid);
		cur_can->eid = NULL;
		cur_can = endpoint_list_free(cur_can);
	}
	return a;
}

struct endpoint_list *endpoint_list_difference(
	struct endpoint_list *a, struct endpoint_list *b, const int free_b)
{
	struct endpoint_list *cur_can = b;

	while (cur_can) {
		endpoint_list_remove(&a, cur_can->eid);
		if (free_b)
			cur_can = endpoint_list_free(cur_can);
		else
			cur_can = cur_can->next;
	}
	return a;
}

int contact_list_sorted(struct contact_list *cl, const int order_by_from)
{
	uint64_t last = 0;

	if (order_by_from) {
		while (cl != NULL) {
			if (cl->data->from_ms < last)
				return 0;
			last = cl->data->from_ms;
			cl = cl->next;
		}
	} else {
		while (cl != NULL) {
			if (cl->data->to_ms < last)
				return 0;
			last = cl->data->to_ms;
			cl = cl->next;
		}
	}
	return 1;
}

struct contact_list *contact_list_free(struct contact_list *e)
{
	struct contact_list *next;

	if (e == NULL)
		return NULL;
	next = e->next;
	free_contact(e->data);
	free(e);
	return next;
}

static struct contact_list *contact_list_free_internal(
	struct contact_list *e, int free_eid_list)
{
	struct contact_list *next;

	if (e == NULL)
		return NULL;
	next = e->next;
	free_contact_internal(e->data, free_eid_list);
	free(e);
	return next;
}

static inline void add_to_modified_list(
	struct contact *c, struct contact_list **modified)
{
	struct contact_list *l;

	if (modified != NULL) {
		l = malloc(sizeof(struct contact_list));
		if (l != NULL) {
			l->next = *modified;
			l->data = c;
			*modified = l;
		}
	}
}

// merges new into old
static inline bool merge_contacts(struct contact *old, struct contact *new)
{
	const uint64_t old_duration_ms = old->to_ms - old->from_ms;

	old->from_ms = MIN(old->from_ms, new->from_ms);
	old->to_ms = MAX(old->to_ms, new->to_ms);

	/* Union EID lists */
	old->contact_endpoints = endpoint_list_union(
		old->contact_endpoints, new->contact_endpoints);
	/* Capacity changed => update bitrate and return true */
	if (old->bitrate_bytes_per_s != new->bitrate_bytes_per_s ||
			old->to_ms - old->from_ms != old_duration_ms) {
		old->bitrate_bytes_per_s = new->bitrate_bytes_per_s;
		recalculate_contact_capacity(old);
		return true;
	}
	return false;
}

struct contact_list *contact_list_union(
	struct contact_list *a, struct contact_list *b,
	struct contact_list **modf)
{
	struct contact_list **cur_slot = &a;
	struct contact_list *cur_can = b;
	struct contact_list *next_can;
	uint64_t cur_from, cur_to;
	bool modified;

	ASSERT(contact_list_sorted(a, 1));
	ASSERT(contact_list_sorted(b, 1));
	if (a == NULL)
		return b;
	else if (b == NULL)
		return a;
	while (*cur_slot != NULL) {
		cur_from = (*cur_slot)->data->from_ms;
		cur_to = (*cur_slot)->data->to_ms;
		ASSERT((*cur_slot)->data->node != NULL);
		/* Traverse b linearly and insert contacts "smaller" than the
		 * current one in a before it.
		 */
		while (cur_can != NULL && cur_can->data->from_ms <= cur_from) {
			next_can = cur_can->next;
			ASSERT(cur_can->data->node != NULL);
			if (cur_can->data->node &&
			    (*cur_slot)->data->node &&
			    strcmp(cur_can->data->node->eid,
				   (*cur_slot)->data->node->eid) == 0) {
				// same node
				if (!contacts_overlap(cur_can->data,
						      (*cur_slot)->data)) {
					// no overlap -> insert before
					cur_can->next = *cur_slot;
					*cur_slot = cur_can;
					cur_slot = &cur_can->next;

				} else {
					// overlap -> merge
					modified = merge_contacts(
						(*cur_slot)->data,
						cur_can->data);
					if (modified)
						add_to_modified_list(
							(*cur_slot)->data,
							modf);
					contact_list_free_internal(cur_can, 0);
				}
			} else {
				// other node, insert before
				cur_can->next = *cur_slot;
				*cur_slot = cur_can;
				cur_slot = &cur_can->next;
			}
			cur_can = next_can;
		}

		// Check rest of cur_can if from < to and same node
		struct contact_list **cur_can_p = &cur_can;

		while (*cur_can_p != NULL) {
			ASSERT((*cur_can_p)->data->node != NULL);
			if ((*cur_can_p)->data->node &&
			    (*cur_slot)->data->node &&
			    (*cur_can_p)->data->from_ms < cur_to &&
			    strcmp((*cur_can_p)->data->node->eid,
				   (*cur_slot)->data->node->eid) == 0) {
				// overlap -> merge
				modified = merge_contacts(
					(*cur_slot)->data,
					(*cur_can_p)->data);
				if (modified)
					add_to_modified_list(
						(*cur_slot)->data,
						modf);
				*cur_can_p = contact_list_free_internal(
					*cur_can_p,
					0
				);
			} else {
				cur_can_p = &(*cur_can_p)->next;
			}
		}

		cur_slot = &(*cur_slot)->next;
	}

	/* Concatenate list ends */
	*cur_slot = cur_can;

	return a;
}

struct contact_list *contact_list_difference(
	struct contact_list *a, struct contact_list *b,
	struct contact_list **modf, struct contact_list **deleted)
{
	struct contact_list **cur_slot = &a;
	struct contact_list *cur_can = b, *l;

	ASSERT(contact_list_sorted(a, 1));
	ASSERT(contact_list_sorted(b, 1));
	if (a == NULL || b == NULL)
		return a;
	while (*cur_slot != NULL) {
		while (*cur_slot != NULL
			&& cur_can != NULL && cur_can->data->from_ms
				<= (*cur_slot)->data->from_ms
		) {
			if (cur_can->data->from_ms == (*cur_slot)->data->from_ms
			    && cur_can->data->to_ms == (*cur_slot)->data->to_ms
			) {
				if (cur_can->data->contact_endpoints == NULL) {
					/* Add to "deleted" list and rm */
					if (deleted != NULL) {
						l = *cur_slot;
						*cur_slot = l->next;
						l->next = *deleted;
						*deleted = l;
					} else if ((*cur_slot)->data->active) {
						l = *cur_slot;
						*cur_slot = l->next;
						free(l);
					} else {
						*cur_slot
						= contact_list_free_internal(
							*cur_slot, 1);
					}
				} else {
					/* Calculate contact difference */
					(*cur_slot)->data->contact_endpoints =
						endpoint_list_difference(
						(*cur_slot)->data
							->contact_endpoints,
						cur_can->data
							->contact_endpoints, 0);
					/* Add to "modified" list */
					add_to_modified_list(
						(*cur_slot)->data, modf);
					cur_slot = &((*cur_slot)->next);
				}
			}
			cur_can = cur_can->next;
		}
		if (*cur_slot != NULL)
			cur_slot = &(*cur_slot)->next;
	}
	return a;
}

struct endpoint_list *endpoint_list_strip_and_sort(struct endpoint_list *el)
{
	struct endpoint_list *cur = el, **i;

	while (cur != NULL) {
		i = &cur->next;
		while (*i != NULL) {
			if (strcmp((*i)->eid, cur->eid) == 0)
				*i = endpoint_list_free(*i);
			else
				i = &(*i)->next;
		}
		cur = cur->next;
	}
	/* Sorted by ptr value */
	LLSORT(struct endpoint_list, eid, el);
	return el;
}

int node_prepare_and_verify(struct node *node)
{
	struct contact_list *cl, *i;

	ASSERT(node != NULL);
	if (!node || node->eid == NULL)
		return 0;

	LLSORT(struct contact_list, data->from_ms, node->contacts);
	cl = node->contacts;
	node->endpoints = endpoint_list_strip_and_sort(node->endpoints);
	while (cl != NULL) {
		if (cl->data->from_ms >= cl->data->to_ms)
			return 0;
		cl->data->contact_endpoints = endpoint_list_strip_and_sort(
			cl->data->contact_endpoints);
		i = cl->next;
		while (i != NULL) {
			if (contacts_overlap(cl->data, i->data))
				return 0;
			i = i->next;
		}
		cl = cl->next;
	}
	return 1;
}

void recalculate_contact_capacity(struct contact *contact)
{
	uint64_t duration_s, new_capacity_bytes;
	int32_t capacity_difference;

	ASSERT(contact != NULL);
	if (!contact)
		return;

	duration_s = (contact->to_ms - contact->from_ms + 500) / 1000;
	new_capacity_bytes = duration_s * contact->bitrate_bytes_per_s;
	// If the calculation overflows or the capacity is > INT32_MAX,
	// we assume an "infinite" contact capacity.
	if ((duration_s != 0 &&
	     new_capacity_bytes / duration_s != contact->bitrate_bytes_per_s) ||
	    new_capacity_bytes >= INT32_MAX) {
		contact->total_capacity_bytes = INT32_MAX;
		contact->remaining_capacity_p0 = INT32_MAX;
		contact->remaining_capacity_p1 = INT32_MAX;
		contact->remaining_capacity_p2 = INT32_MAX;
		return;
	}
	capacity_difference = (
		new_capacity_bytes -
		(int32_t)contact->total_capacity_bytes
	);
	contact->total_capacity_bytes = (uint32_t)new_capacity_bytes;
	contact->remaining_capacity_p0 += capacity_difference;
	contact->remaining_capacity_p1 += capacity_difference;
	contact->remaining_capacity_p2 += capacity_difference;
}

int32_t contact_get_remaining_capacity_bytes(
	struct contact *contact, enum bundle_routing_priority prio,
	uint64_t time_ms)
{
	uint64_t cap_left;
	int32_t cap_result;

	ASSERT(contact != NULL);
	if (!contact)
		return 0;

	if (time_ms >= contact->to_ms)
		return 0;
	if (time_ms <= contact->from_ms)
		return CONTACT_CAPACITY(contact, prio);
	if (contact->total_capacity_bytes >= INT32_MAX)
		return INT32_MAX;
	cap_left = (
		contact->total_capacity_bytes *
		(uint64_t)(contact->to_ms - time_ms) /
		(uint64_t)(contact->to_ms - contact->from_ms)
	);
	cap_result = cap_left;
	return MIN(cap_result, CONTACT_CAPACITY(contact, prio));
}

int32_t contact_get_cur_remaining_capacity_bytes(
	struct contact *contact, enum bundle_routing_priority prio)
{
	return contact_get_remaining_capacity_bytes(
		contact,
		prio,
		hal_time_get_timestamp_ms()
	);
}

int add_contact_to_ordered_list(
	struct contact_list **list, struct contact *contact,
	const int order_by_from)
{
	struct contact_list **cur_entry, *new_entry;

	ASSERT(list != NULL);
	ASSERT(contact != NULL);
	if (!list || !contact)
		return 0;

	cur_entry = list;
	while (*cur_entry != NULL) {
		if ((*cur_entry)->data == contact)
			return 0;
		if (order_by_from) {
			if ((*cur_entry)->data->from_ms > contact->from_ms)
				break;
		} else {
			if ((*cur_entry)->data->to_ms > contact->to_ms)
				break;
		}
		cur_entry = &(*cur_entry)->next;
	}
	new_entry = malloc(sizeof(struct contact_list));
	if (new_entry == NULL)
		return 0;
	new_entry->data = contact;
	new_entry->next = *cur_entry;
	*cur_entry = new_entry;
	return 1;
}

int remove_contact_from_list(
	struct contact_list **list, struct contact *contact)
{
	struct contact_list **cur_entry, *tmp;

	ASSERT(list != NULL);
	ASSERT(contact != NULL);
	if (!list || !contact)
		return 0;

	cur_entry = list;
	while (*cur_entry != NULL) {
		if ((*cur_entry)->data == contact) {
			tmp = *cur_entry;
			*cur_entry = (*cur_entry)->next;
			free(tmp);
			return 1;
		}
		cur_entry = &(*cur_entry)->next;
	}
	return 0;
}
