#include "ud3tn/bundle.h"
#include "ud3tn/bundle_processor.h"
#include "ud3tn/bundle_storage_manager.h"
#include "ud3tn/common.h"
#include "ud3tn/node.h"
#include "ud3tn/router.h"
#include "ud3tn/routing_table.h"
#include "ud3tn/simplehtab.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static struct node_list *node_list;
static struct contact_list *contact_list;

static struct htab_entrylist *htab_elem[NODE_HTAB_SLOT_COUNT];
static struct htab eid_table;
static uint8_t eid_table_initialized;

/* INIT */

enum ud3tn_result routing_table_init(void)
{
	if (eid_table_initialized != 0)
		return UD3TN_OK;
	node_list = NULL;
	contact_list = NULL;
	htab_init(&eid_table, NODE_HTAB_SLOT_COUNT, htab_elem);
	eid_table_initialized = 1;
	return UD3TN_OK;
}

void routing_table_free(void)
{
	struct node_list *next;

	while (contact_list != NULL)
		routing_table_delete_contact(contact_list->data);
	while (node_list != NULL) {
		free_node(node_list->node);
		next = node_list->next;
		free(node_list);
		node_list = next;
	}
}

/* LOOKUP */

static struct node_list **get_node_entry_ptr_by_eid(
	const char *eid)
{
	struct node_list **cur = &node_list;

	if (eid == NULL)
		return NULL;
	/* Linear search for now */
	while ((*cur) != NULL) {
		if (strcmp((*cur)->node->eid, eid) == 0)
			return cur;
		cur = &((*cur)->next);
	}
	return NULL;
}

static struct node_list *get_node_entry_by_eid(
	const char *eid)
{
	struct node_list **entry_ptr
		= get_node_entry_ptr_by_eid(eid);

	if (entry_ptr == NULL)
		return NULL;
	else
		return *entry_ptr;
}

struct node *routing_table_lookup_node(const char *eid)
{
	struct node_list *entry = get_node_entry_by_eid(eid);

	if (entry == NULL)
		return NULL;
	else
		return entry->node;
}

struct node_table_entry *routing_table_lookup_eid(const char *eid)
{
	return (struct node_table_entry *)htab_get(&eid_table, eid);
}


uint8_t routing_table_lookup_hot_node(
	struct node **target, uint8_t max)
{
	struct node_list *cur = node_list;
	uint8_t c = 0;

	while (cur != NULL) {
		if (HAS_FLAG(cur->node->flags, NODE_FLAG_INTERNET_ACCESS)) {
			target[c] = cur->node;
			if (++c == max)
				break;
		}
		cur = cur->next;
	}
	return c;
}

/* NODE LIST MODIFICATION */
static void add_node_to_tables(struct node *node);
static void remove_node_from_tables(struct node *node, bool drop_contacts,
				  QueueIdentifier_t bproc_signaling_queue);

static void reschedule_bundles(
	struct contact *contact, QueueIdentifier_t bproc_signaling_queue);

static bool add_new_node(struct node *new_node)
{
	struct node_list *new_elem;

	if (new_node->eid == NULL || new_node->cla_addr == NULL) {
		free_node(new_node);
		return false;
	}

	new_elem = malloc(sizeof(struct node_list));
	if (new_elem == NULL) {
		free_node(new_node);
		return false;
	}
	new_elem->node = new_node;
	new_elem->next = node_list;
	node_list = new_elem;

	add_node_to_tables(new_node);
	return true;
}

bool routing_table_add_node(
	struct node *new_node, QueueIdentifier_t bproc_signaling_queue)
{
	struct node_list *entry;
	struct node *cur_node;
	struct contact_list *cap_modified = NULL, *cur_contact, *next;

	ASSERT(new_node != NULL);
	entry = get_node_entry_by_eid(new_node->eid);

	if (entry == NULL)
		return add_new_node(new_node);

	cur_node = entry->node;
	/* Should not be needed here because we only ADD */
	/* remove_node_from_htab(cur_node); */
	if (new_node->cla_addr != NULL &&
		new_node->cla_addr[0] != '\0') {
		// New non-empty CLA address provided
		free(cur_node->cla_addr);
		cur_node->cla_addr = new_node->cla_addr;
	} else {
		free(new_node->cla_addr);
	}
	cur_node->endpoints = endpoint_list_union(
		cur_node->endpoints, new_node->endpoints);
	cur_node->contacts = contact_list_union(
		cur_node->contacts, new_node->contacts,
		&cap_modified);
	/* Fix node assignment for all contacts */
	/* (normally only needed for new contacts) */
	cur_contact = cur_node->contacts;
	while (cur_contact != NULL) {
		cur_contact->data->node = cur_node;
		cur_contact = cur_contact->next;
	}
	/* Process contacts with modified capacity */
	while (cap_modified != NULL) {
		/* Check for all nodes with modified bandwidth if now */
		/* the remaining capacity for p0 is negative: */
		/* We assume that the new contact capacity */
		/* has already been calculated */
		/* TODO: This can be improved! */
		if (cap_modified->data->remaining_capacity_p0 < 0) {
			reschedule_bundles(
				cap_modified->data,
				bproc_signaling_queue
			);
		}
		next = cap_modified->next;
		free(cap_modified);
		cap_modified = next;
	}
	add_node_to_tables(cur_node);
	free(new_node->eid);
	free(new_node);
	return true;
}

bool routing_table_replace_node(
	struct node *node, QueueIdentifier_t bproc_signaling_queue)
{
	struct node_list *entry;

	ASSERT(node != NULL);
	entry = get_node_entry_by_eid(node->eid);

	if (entry == NULL)
		return false;

	remove_node_from_tables(entry->node, true,
				bproc_signaling_queue);
	free_node(entry->node);
	entry->node = node;
	add_node_to_tables(node);
	return true;
}

bool routing_table_delete_node_by_eid(
	char *eid, QueueIdentifier_t bproc_signaling_queue)
{
	struct node_list **entry_ptr, *old_node_entry;

	ASSERT(eid != NULL);
	entry_ptr = get_node_entry_ptr_by_eid(eid);
	if (entry_ptr != NULL) {
		/* Delete whole node */
		old_node_entry = *entry_ptr;
		*entry_ptr = old_node_entry->next;
		remove_node_from_tables(old_node_entry->node, true,
					bproc_signaling_queue);
		free_node(old_node_entry->node);
		free(old_node_entry);
		return true;
	}
	return false;
}

bool routing_table_delete_node(
	struct node *new_node, QueueIdentifier_t bproc_signaling_queue)
{
	struct node_list **entry_ptr, *old_node_entry;
	struct node *cur_node;
	struct contact_list *modified = NULL, *deleted = NULL, *next, *tmp;

	ASSERT(new_node != NULL);
	entry_ptr = get_node_entry_ptr_by_eid(new_node->eid);
	if (entry_ptr != NULL) {
		cur_node = (*entry_ptr)->node;
		if (new_node->endpoints == NULL && new_node->contacts == NULL) {
			/* Delete whole node */
			old_node_entry = *entry_ptr;
			*entry_ptr = old_node_entry->next;
			remove_node_from_tables(old_node_entry->node, true,
						bproc_signaling_queue);
			free_node(old_node_entry->node);
			free(old_node_entry);
			free_node(new_node);
		} else {
			/* Delete contacts/nodes */
			remove_node_from_tables(cur_node, false, NULL);
			cur_node->endpoints = endpoint_list_difference(
				cur_node->endpoints, new_node->endpoints, 1);
			cur_node->contacts = contact_list_difference(
				cur_node->contacts, new_node->contacts, 1,
				&modified, &deleted);
			/* Process modified contacts */
			while (modified != NULL) {
				reschedule_bundles(
					modified->data, bproc_signaling_queue);
				next = modified->next;
				free(modified);
				modified = next;
			}
			/* Process deleted contacts */
			while (deleted != NULL) {
				reschedule_bundles(
					deleted->data, bproc_signaling_queue);
				if (deleted->data->active) {
					tmp = deleted;
					deleted = tmp->next;
					free(tmp);
				} else {
					deleted = contact_list_free(deleted);
				}
			}
			add_node_to_tables(cur_node);
			free(new_node->eid);
			free(new_node);
		}
		return true;
	}
	free_node(new_node);
	return false;
}

static bool add_contact_to_node_in_htab(char *eid, struct contact *c, float p);
static bool remove_contact_from_node_in_htab(char *eid, struct contact *c);
static bool check_for_invalid_overlaps(struct contact *c);

static void add_node_to_tables(struct node *node)
{
	struct contact_list *cur_contact;
	struct endpoint_list *cur_persistent_node, *cur_contact_node;

	ASSERT(node != NULL);
	cur_contact = node->contacts;
	while (cur_contact != NULL) {
		/* TODO: Try to add contact but reduce timespan */
		if (check_for_invalid_overlaps(cur_contact->data)) {
			cur_contact = cur_contact->next;
			continue;
		}
		add_contact_to_node_in_htab(node->eid, cur_contact->data, 1.0f);
		cur_persistent_node = node->endpoints;
		while (cur_persistent_node != NULL) {
			add_contact_to_node_in_htab(
				cur_persistent_node->eid, cur_contact->data,
				1.0f);
			cur_persistent_node = cur_persistent_node->next;
		}
		cur_contact_node = cur_contact->data->contact_endpoints;
		while (cur_contact_node != NULL) {
			add_contact_to_node_in_htab(
				cur_contact_node->eid, cur_contact->data,
				1.0f);
			cur_contact_node = cur_contact_node->next;
		}
		add_contact_to_ordered_list(
			&contact_list, cur_contact->data, 1);
		recalculate_contact_capacity(cur_contact->data);
		cur_contact = cur_contact->next;
	}
}

static void remove_node_from_tables(struct node *node, bool drop_contacts,
				  QueueIdentifier_t bproc_signaling_queue)
{
	struct contact_list **cur_slot;
	struct endpoint_list *cur_persistent_node, *cur_contact_node;

	ASSERT(node != NULL);
	cur_slot = &node->contacts;
	while (*cur_slot != NULL) {
		struct contact_list *const cur_contact = *cur_slot;

		remove_contact_from_node_in_htab(node->eid, cur_contact->data);
		cur_persistent_node = node->endpoints;
		while (cur_persistent_node != NULL) {
			remove_contact_from_node_in_htab(
				cur_persistent_node->eid, cur_contact->data);
			cur_persistent_node = cur_persistent_node->next;
		}
		cur_contact_node = cur_contact->data->contact_endpoints;
		while (cur_contact_node != NULL) {
			remove_contact_from_node_in_htab(
				cur_contact_node->eid, cur_contact->data);
			cur_contact_node = cur_contact_node->next;
		}
		remove_contact_from_list(&contact_list, cur_contact->data);
		if (drop_contacts) {
			reschedule_bundles(cur_contact->data,
					   bproc_signaling_queue);
			// If the contact is active, un-associate it to prevent
			// freeing it right now.
			if (cur_contact->data->active) {
				cur_contact->data->node = NULL;
				*cur_slot = cur_contact->next;
				free(cur_contact);
				// List item was replaced by next item,
				// process this one now...
				continue;
			}
		}
		cur_slot = &(*cur_slot)->next;
	}
}

static bool add_contact_to_node_in_htab(char *eid, struct contact *c, float p)
{
	struct node_table_entry *entry;

	ASSERT(eid != NULL);
	ASSERT(p > 0.0f && p <= 1.0f);
	ASSERT(c != NULL);
	entry = (struct node_table_entry *)htab_get(&eid_table, eid);
	if (entry == NULL) {
		entry = malloc(sizeof(struct node_table_entry));
		if (entry == NULL)
			return false;
		entry->ref_count = 0;
		entry->contacts = NULL;
		htab_add(&eid_table, eid, entry);
	}
	if (add_contact_to_ordered_assoc_list(&(entry->contacts), c, p, 0)) {
		entry->ref_count++;
		return true;
	}
	return false;
}

static bool remove_contact_from_node_in_htab(char *eid, struct contact *c)
{
	struct node_table_entry *entry;

	ASSERT(eid != NULL);
	ASSERT(c != NULL);
	entry = (struct node_table_entry *)htab_get(&eid_table, eid);
	if (entry == NULL)
		return false;
	if (remove_contact_from_assoc_list(&(entry->contacts), c)) {
		entry->ref_count--;
		if (entry->ref_count <= 0) {
			htab_remove(&eid_table, eid);
			free(entry);
		}
		return true;
	}
	return false;
}

static bool check_for_invalid_overlaps(struct contact *c)
{
	uint8_t overlaps = 0;
	struct contact_list *cur = contact_list;

	while (cur != NULL && cur->data->to < c->from)
		cur = cur->next;
	if (cur == NULL || cur->data->from >= c->to)
		return false;
	do {
		if (cur->data->to > c->from) {
			/*
			 * There cannot be two overlapping contacts with
			 * the same ground station b/c this would use the
			 * same CLA channel.
			 */
			if (cur->data->node == c->node)
				return true;
			overlaps++;
		}
		cur = cur->next;
	} while (cur != NULL && cur->data->from < c->to);
	return (bool)(overlaps >= MAX_CONCURRENT_CONTACTS);
}

/* CONTACT LIST */
struct contact_list **routing_table_get_raw_contact_list_ptr(void)
{
	return &contact_list;
}

struct node_list *routing_table_get_node_list(void)
{
	return node_list;
}

void routing_table_delete_contact(struct contact *contact)
{
	struct endpoint_list *cur_eid;

	ASSERT(contact != NULL);
	ASSERT(contact->contact_bundles == NULL);
	if (contact->node != NULL) {
		remove_contact_from_node_in_htab(
			contact->node->eid, contact);
		/* Remove contact from reachable endpoints */
		cur_eid = contact->node->endpoints;
		while (cur_eid != NULL) {
			remove_contact_from_node_in_htab(
				cur_eid->eid, contact);
			cur_eid = cur_eid->next;
		}
		/* Remove from node list */
		remove_contact_from_list(
			&contact->node->contacts, contact);
	}
	/* Remove contact from contact nodes and free list */
	cur_eid = contact->contact_endpoints;
	while (cur_eid != NULL) {
		remove_contact_from_node_in_htab(
			cur_eid->eid, contact);
		cur_eid = endpoint_list_free(cur_eid);
	}
	contact->contact_endpoints = NULL;
	/* Remove from global list */
	remove_contact_from_list(&contact_list, contact);
	/* Free contact itself */
	free_contact(contact);
}

void routing_table_contact_passed(
	struct contact *contact, QueueIdentifier_t bproc_signaling_queue)
{
	struct routed_bundle_list *tmp;

	if (contact->node != NULL) {
		while (contact->contact_bundles != NULL) {
			/* TODO: Transmit struct routed_bundle? */
			bundle_processor_inform(
				bproc_signaling_queue,
				contact->contact_bundles->data->id,
				BP_SIGNAL_RESCHEDULE_BUNDLE,
				BUNDLE_SR_REASON_NO_INFO);
			tmp = contact->contact_bundles->next;
			free(contact->contact_bundles->data);
			free(contact->contact_bundles);
			contact->contact_bundles = tmp;
		}
	}
	routing_table_delete_contact(contact);
}

/* RE-SCHEDULING */

static void reschedule_bundles(
	struct contact *contact, QueueIdentifier_t bproc_signaling_queue)
{
	struct routed_bundle *rb;
	struct fragment_route fr;
	bundleid_t id;
	uint8_t c;

	ASSERT(contact != NULL);
	/* Empty the bundle list and queue them in for re-scheduling */
	while (contact->contact_bundles != NULL) {
		rb = contact->contact_bundles->data;
		ASSERT(rb->contact_count <= ROUTER_MAX_CONTACTS);
		for (c = 0; c < rb->contact_count; c++)
			fr.contacts[c] = rb->contacts[c];
		fr.contact_count = rb->contact_count;
		fr.payload_size = rb->size;
		id = rb->id;
		router_remove_bundle_from_route(&fr, id, 1);
		bundle_processor_inform(
			bproc_signaling_queue, id,
			BP_SIGNAL_RESCHEDULE_BUNDLE,
			BUNDLE_SR_REASON_NO_INFO
		);
	}
}
