#include "ud3tn/bundle_processor.h"
#include "ud3tn/node.h"
#include "ud3tn/routing_table.h"

#include "platform/hal_queue.h"
#include "platform/hal_time.h"

#include "util/llsort.h"

#include "unity_fixture.h"

#include <stdlib.h>
#include <string.h>

TEST_GROUP(routingTable);

static struct contact *createct(struct node *node,
	uint64_t from, uint64_t to, uint16_t bitrate)
{
	struct contact *c = contact_create(node);

	c->from = from;
	c->to = to;
	c->bitrate = bitrate;
	recalculate_contact_capacity(c);
	return c;
}

static void addnode(struct endpoint_list **list, char *node)
{
	struct endpoint_list *l;

	l = malloc(sizeof(struct endpoint_list));
	l->eid = strdup(node);
	l->next = *list;
	*list = l;
}

static struct node *node11, *node1_no_cla1, *node1_no_cla2, *node12, *node13;
static struct node *node2, *node3, *node4;
static struct contact *c1, *c2, *c3, *c4, *c5, *c6, *c7,
	*c8, *c9, *c10, *c11, *c12;
static QueueIdentifier_t sig_queue;

TEST_SETUP(routingTable)
{
	/* node1-1 */
	node11 = node_create("node1");
	node11->cla_addr = strdup("cla:addr1");
	addnode(&node11->endpoints, "node1");
	c1 = createct(node11, 1, 2, 200);
	addnode(&c1->contact_endpoints, "node2");
	c2 = createct(node11, 2, 3, 300);
	addnode(&c2->contact_endpoints, "node4");
	c3 = createct(node11, 4, 5, 400);
	add_contact_to_ordered_list(&node11->contacts, c1, 1);
	add_contact_to_ordered_list(&node11->contacts, c2, 1);
	add_contact_to_ordered_list(&node11->contacts, c3, 1);
	/* node1-no-cla-address */
	node1_no_cla1 = node_create("node1");
	node1_no_cla1->cla_addr = NULL;
	node1_no_cla2 = node_create("node1");
	node1_no_cla2->cla_addr = NULL;
	/* node1-2 */
	node12 = node_create("node1");
	node12->cla_addr = strdup("cla:addr2");
	addnode(&node12->endpoints, "node3");
	c4 = createct(node12, 5, 6, 500);
	c5 = createct(node12, 2, 3, 600);
	c6 = createct(node12, 4, 6, 700);
	add_contact_to_ordered_list(&node12->contacts, c4, 1);
	add_contact_to_ordered_list(&node12->contacts, c5, 1);
	add_contact_to_ordered_list(&node12->contacts, c6, 1);
	/* node1-3 */
	node13 = node_create("node1");
	node13->cla_addr = strdup("cla:addr3");
	addnode(&node13->endpoints, "node3");
	c9 = createct(node13, 1, 2, 200);
	c10 = createct(node13, 2, 3, 300);
	c11 = createct(node13, 5, 6, 500); // invalid contact (overlapping)
	c12 = createct(node13, 4, 6, 700);
	add_contact_to_ordered_list(&node13->contacts, c9, 0);
	add_contact_to_ordered_list(&node13->contacts, c10, 0);
	add_contact_to_ordered_list(&node13->contacts, c11, 0);
	add_contact_to_ordered_list(&node13->contacts, c12, 0);
	/* node2 */
	node2 = node_create("node2");
	node2->cla_addr = strdup("cla:addr4");
	addnode(&node2->endpoints, "node3");
	c7 = createct(node2, 8, 9, 800);
	c8 = createct(node2, 6, 8, 900);
	addnode(&c8->contact_endpoints, "node5");
	add_contact_to_ordered_list(&node2->contacts, c7, 1);
	add_contact_to_ordered_list(&node2->contacts, c8, 1);
	/* node3 */
	node3 = node_create("node3");
	node3->cla_addr = strdup("cla:addr5");
	addnode(&node3->endpoints, "node6");
	/* node4 */
	node4 = node_create("node4");
	/* queue */
	sig_queue = hal_queue_create(60, 6);
	/* init rt */
	routing_table_init();
	/* clock */
	hal_time_init(0);
}

TEST_TEAR_DOWN(routingTable)
{
	routing_table_free();
	hal_queue_delete(sig_queue);
}

TEST(routingTable, routing_table_invalid_cla)
{
	TEST_ASSERT_FALSE(routing_table_add_node(node1_no_cla1, sig_queue));
	TEST_ASSERT_FALSE(routing_table_replace_node(node1_no_cla2, sig_queue));
	TEST_ASSERT_FALSE(routing_table_delete_node(node11, sig_queue));
}

TEST(routingTable, routing_table_add_delete)
{
	struct node_table_entry *nti;

	TEST_ASSERT_TRUE(routing_table_add_node(node11, sig_queue));
	TEST_ASSERT_TRUE(routing_table_add_node(node2, sig_queue));
	TEST_ASSERT_TRUE(routing_table_add_node(node12, sig_queue));
	TEST_ASSERT_TRUE(routing_table_add_node(node3, sig_queue));
	// Replaced/updated node has to exist, thus, REPLACE returns false
	TEST_ASSERT_FALSE(routing_table_replace_node(node4, sig_queue));
	TEST_ASSERT_EQUAL_PTR(node11, routing_table_lookup_node("node1"));
	TEST_ASSERT_EQUAL_PTR(node2, routing_table_lookup_node("node2"));
	TEST_ASSERT_EQUAL_PTR(node3, routing_table_lookup_node("node3"));
	/* check lookup */
	nti = routing_table_lookup_eid("node2");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(3, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c1, nti->contacts->data);
	TEST_ASSERT_NOT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->next->data);
	TEST_ASSERT_EQUAL_PTR(c8, nti->contacts->next->data);
	TEST_ASSERT_NOT_NULL(nti->contacts->next->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->next->next->data);
	TEST_ASSERT_EQUAL_PTR(c7, nti->contacts->next->next->data);
	TEST_ASSERT_NULL(nti->contacts->next->next->next);
	/* check updated bitrate (merging of c1-1, c1-2) */
	nti = routing_table_lookup_eid("node4");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(1, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c2, nti->contacts->data);
	TEST_ASSERT_EQUAL_UINT16(600, nti->contacts->data->bitrate);
	/* check some other nodes */
	nti = routing_table_lookup_eid("node5");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(1, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c8, nti->contacts->data);
	nti = routing_table_lookup_eid("node6");
	TEST_ASSERT_NULL(nti);
	/* delete */
	LLSORT(struct contact_list, data->from, node13->contacts);
	TEST_ASSERT_TRUE(routing_table_delete_node(node13, sig_queue));
	nti = routing_table_lookup_eid("node1");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(1, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c3, nti->contacts->data);
	nti = routing_table_lookup_eid("node2");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(2, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c8, nti->contacts->data);
	TEST_ASSERT_NOT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->next->data);
	TEST_ASSERT_EQUAL_PTR(c7, nti->contacts->next->data);
	TEST_ASSERT_NULL(nti->contacts->next->next);
	nti = routing_table_lookup_eid("node4");
	TEST_ASSERT_NULL(nti);
	/* should be reachable only via node2 */
	nti = routing_table_lookup_eid("node3");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(2, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NOT_NULL(nti->contacts->next);
	TEST_ASSERT_NULL(nti->contacts->next->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_NOT_NULL(nti->contacts->next->data);
	TEST_ASSERT_EQUAL_PTR(c8, nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c7, nti->contacts->next->data);
	/* remove contact */
	routing_table_delete_contact(c8);
	nti = routing_table_lookup_eid("node3");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(1, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c7, nti->contacts->data);
	TEST_ASSERT_TRUE(routing_table_delete_node(
		node_create("node1"), sig_queue));
	TEST_ASSERT_TRUE(routing_table_delete_node(
		node_create("node2"), sig_queue));
	TEST_ASSERT_TRUE(routing_table_delete_node(
		node_create("node3"), sig_queue));
}

TEST(routingTable, routing_table_replace)
{
	struct node_table_entry *nti;

	LLSORT(struct contact_list, data->from, node13->contacts);
	TEST_ASSERT_TRUE(routing_table_add_node(node13, sig_queue));
	nti = routing_table_lookup_eid("node3");
	TEST_ASSERT_EQUAL_UINT16(3, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_EQUAL_PTR(c12, nti->contacts->next->next->data);
	TEST_ASSERT_TRUE(routing_table_replace_node(node11, sig_queue));
	nti = routing_table_lookup_eid("node3");
	TEST_ASSERT_NULL(nti);
	TEST_ASSERT_NULL(routing_table_lookup_node("node2"));
	TEST_ASSERT_TRUE(routing_table_delete_node(
		node_create("node1"),
		sig_queue
	));
	free_node(node12);
	free_node(node2);
	free_node(node3);
}

TEST_GROUP_RUNNER(routingTable)
{
	RUN_TEST_CASE(routingTable, routing_table_add_delete);
	RUN_TEST_CASE(routingTable, routing_table_replace);
}
