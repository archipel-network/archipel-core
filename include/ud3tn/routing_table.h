// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef ROUTINGTABLE_H_INCLUDED
#define ROUTINGTABLE_H_INCLUDED

#include "ud3tn/bundle.h"
#include "ud3tn/node.h"
#include "ud3tn/result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Number of slots in the node hash table.
#ifndef NODE_HTAB_SLOT_COUNT
#define NODE_HTAB_SLOT_COUNT 128
#endif // NODE_HTAB_SLOT_COUNT

struct node_table_entry {
	uint16_t ref_count;
	struct contact_list *contacts;
};

typedef void (*reschedule_func_t)(
	struct bundle *,
	const void *reschedule_func_context
);
struct rescheduling_handle {
	reschedule_func_t reschedule_func;
	const void *reschedule_func_context;
};

enum ud3tn_result routing_table_init(void);
void routing_table_free(void);

struct node *routing_table_lookup_node(const char *eid);
struct node_table_entry *routing_table_lookup_eid(const char *eid);
uint8_t routing_table_lookup_eid_in_nbf(
	char *eid, struct node **target, uint8_t max);
uint8_t routing_table_lookup_hot_node(
	struct node **target, uint8_t max);

bool routing_table_add_node(
	struct node *new_node, struct rescheduling_handle rescheduler);
bool routing_table_replace_node(
	struct node *node, struct rescheduling_handle rescheduler);
bool routing_table_delete_node(
	struct node *new_node, struct rescheduling_handle rescheduler);
bool routing_table_delete_node_by_eid(
	char *eid, struct rescheduling_handle rescheduler);

struct contact_list **routing_table_get_raw_contact_list_ptr(void);
struct node_list *routing_table_get_node_list(void);
void routing_table_delete_contact(struct contact *contact);
void routing_table_contact_passed(
	struct contact *contact, struct rescheduling_handle rescheduler);

#endif /* ROUTINGTABLE_H_INCLUDED */
