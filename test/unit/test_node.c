#include "ud3tn/node.h"

#include "platform/hal_time.h"

#include "unity_fixture.h"

#include <stdlib.h>
#include <string.h>

TEST_GROUP(node);

static struct endpoint_list *some_eids1;
static struct endpoint_list *some_eids2;
static struct contact_list *some_ct1;
static struct contact_list *some_ct2;

static struct contact *createct(uint64_t from, uint64_t to, uint16_t bitrate)
{
	struct contact *c = contact_create(NULL);

	c->from = from;
	c->to = to;
	c->bitrate = bitrate;
	recalculate_contact_capacity(c);
	return c;
}

TEST_SETUP(node)
{
	/* eids */
	char *e1;
	char *e2;
	char *e3;
	char *e4;

	e1 = strdup("testeid1");
	e2 = strdup("testeid2");
	e3 = strdup("testeid2");
	e4 = strdup("testeid3");
	some_eids1 = malloc(sizeof(struct endpoint_list));
	some_eids1->eid = e1;
	some_eids1->next = malloc(sizeof(struct endpoint_list));
	some_eids1->next->eid = e2;
	some_eids1->next->next = NULL;
	some_eids1 = endpoint_list_strip_and_sort(some_eids1);
	some_eids2 = malloc(sizeof(struct endpoint_list));
	some_eids2->eid = e3;
	some_eids2->next = malloc(sizeof(struct endpoint_list));
	some_eids2->next->eid = e4;
	some_eids2->next->next = NULL;
	some_eids2 = endpoint_list_strip_and_sort(some_eids2);
	/* contacts */
	some_ct1 = malloc(sizeof(struct contact_list));
	some_ct1->data = createct(1, 3, 300);
	some_ct1->next = malloc(sizeof(struct contact_list));
	some_ct1->next->data = createct(0x100000000, 0x100000001, 500);
	some_ct1->next->next = NULL;
	some_ct2 = malloc(sizeof(struct contact_list));
	some_ct2->data = createct(0x100000000, 0x100000001, 600);
	some_ct2->next = NULL;
	/* time */
	hal_time_init(0);
}

TEST_TEAR_DOWN(node)
{
	while (some_eids1 != NULL)
		some_eids1 = endpoint_list_free(some_eids1);
	while (some_eids2 != NULL)
		some_eids2 = endpoint_list_free(some_eids2);
	while (some_ct1 != NULL)
		some_ct1 = contact_list_free(some_ct1);
	while (some_ct2 != NULL)
		some_ct2 = contact_list_free(some_ct2);
}

TEST(node, contact)
{
	/* capacity */
	TEST_ASSERT_EQUAL_UINT32(500, some_ct1->next->data->total_capacity);
	TEST_ASSERT_EQUAL_INT32(500,
		some_ct1->next->data->remaining_capacity_p0);
	/* re-calculation accuracy */
	recalculate_contact_capacity(some_ct1->next->data);
	recalculate_contact_capacity(some_ct1->next->data);
	TEST_ASSERT_EQUAL_UINT32(500, some_ct1->next->data->total_capacity);
	TEST_ASSERT_EQUAL_INT32(500,
		some_ct1->next->data->remaining_capacity_p0);
	/* remaining cap */
	hal_time_init(0);
	TEST_ASSERT_EQUAL_INT32(600,
		contact_get_cur_remaining_capacity(some_ct1->data, 0));
	hal_time_init(2);
	TEST_ASSERT_EQUAL_INT32(300,
		contact_get_cur_remaining_capacity(some_ct1->data, 0));
	hal_time_init(3);
	TEST_ASSERT_EQUAL_INT32(0,
		contact_get_cur_remaining_capacity(some_ct1->data, 0));
}

static void assert_in_eidlist(char *eid, struct endpoint_list *l)
{
	int r = 0;

	while (l != NULL) {
		if (strcmp(l->eid, eid) == 0)
			r = 1;
		l = l->next;
	}
	TEST_ASSERT_TRUE(r);
}

TEST(node, endpoint_list_union)
{
	some_eids1 = endpoint_list_union(some_eids1, some_eids2);
	some_eids2 = NULL;
	TEST_ASSERT_NOT_NULL(some_eids1);
	TEST_ASSERT_NOT_NULL(some_eids1->next);
	TEST_ASSERT_NOT_NULL(some_eids1->next->next);
	assert_in_eidlist("testeid3", some_eids1);
	assert_in_eidlist("testeid1", some_eids1);
	assert_in_eidlist("testeid2", some_eids1);
	TEST_ASSERT_NULL(some_eids1->next->next->next);
}

TEST(node, endpoint_list_difference)
{
	some_eids1 = endpoint_list_difference(some_eids1, some_eids2, 1);
	some_eids2 = NULL;
	TEST_ASSERT_NOT_NULL(some_eids1);
	TEST_ASSERT_NULL(some_eids1->next);
	TEST_ASSERT_EQUAL_STRING("testeid1", some_eids1->eid);
}

TEST(node, contact_list_union)
{
	struct contact_list *mod = NULL;

	some_ct1 = contact_list_union(some_ct1, some_ct2, &mod);
	some_ct2 = NULL;
	TEST_ASSERT_NOT_NULL(some_ct1);
	TEST_ASSERT_NOT_NULL(some_ct1->next);
	TEST_ASSERT_NULL(some_ct1->next->next);
	TEST_ASSERT_EQUAL_UINT64(1, some_ct1->data->from);
	TEST_ASSERT_EQUAL_UINT64(3, some_ct1->data->to);
	TEST_ASSERT_EQUAL_UINT16(300, some_ct1->data->bitrate);
	TEST_ASSERT_EQUAL_UINT32(600, some_ct1->data->total_capacity);
	TEST_ASSERT_EQUAL_INT32(
		600, some_ct1->data->remaining_capacity_p0);
	TEST_ASSERT_EQUAL_HEX64(0x100000000, some_ct1->next->data->from);
	TEST_ASSERT_EQUAL_HEX64(0x100000001, some_ct1->next->data->to);
	TEST_ASSERT_EQUAL_UINT16(600, some_ct1->next->data->bitrate);
	TEST_ASSERT_EQUAL_UINT32(600, some_ct1->next->data->total_capacity);
	TEST_ASSERT_EQUAL_INT32(600,
		some_ct1->next->data->remaining_capacity_p0);
	TEST_ASSERT_NOT_NULL(mod);
	TEST_ASSERT_NULL(mod->next);
	TEST_ASSERT_EQUAL_PTR(some_ct1->next->data, mod->data);
	free(mod); /* data is freed by tear_down */
}

TEST(node, contact_list_difference)
{
	struct contact_list *mod = NULL;
	struct contact_list *del = NULL;

	some_ct1 = contact_list_difference(some_ct1, some_ct2, 1, &mod, &del);
	some_ct2 = NULL;
	TEST_ASSERT_NOT_NULL(some_ct1);
	TEST_ASSERT_NULL(some_ct1->next);
	TEST_ASSERT_EQUAL_UINT64(1, some_ct1->data->from);
	TEST_ASSERT_EQUAL_UINT64(3, some_ct1->data->to);
	TEST_ASSERT_EQUAL_UINT16(300, some_ct1->data->bitrate);
	TEST_ASSERT_EQUAL_UINT32(600, some_ct1->data->total_capacity);
	TEST_ASSERT_EQUAL_INT32(600, some_ct1->data->remaining_capacity_p0);
	TEST_ASSERT_NULL(mod);
	TEST_ASSERT_NOT_NULL(del);
	TEST_ASSERT_NULL(del->next);
	TEST_ASSERT_EQUAL_HEX64(0x100000000, del->data->from);
	TEST_ASSERT_EQUAL_HEX64(0x100000001, del->data->to);
	TEST_ASSERT_EQUAL_UINT16(500, del->data->bitrate);
	TEST_ASSERT_EQUAL_UINT32(500, del->data->total_capacity);
	TEST_ASSERT_EQUAL_INT32(500, del->data->remaining_capacity_p0);
	del = contact_list_free(del);
	TEST_ASSERT_NULL(del);
}

TEST(node, add_contact_to_ordered_list)
{
	struct contact *c1 = createct(100, 500, 1);
	struct contact *c2 = createct(200, 600, 1);
	struct contact *c3 = createct(300, 400, 1);
	struct contact_list *l = NULL;

	TEST_ASSERT_TRUE(add_contact_to_ordered_list(&l, c1, 0));
	TEST_ASSERT_TRUE(add_contact_to_ordered_list(&l, c2, 0));
	TEST_ASSERT_TRUE(add_contact_to_ordered_list(&l, c3, 0));
	TEST_ASSERT_FALSE(add_contact_to_ordered_list(&l, c3, 0));
	TEST_ASSERT_NOT_NULL(l);
	TEST_ASSERT_NOT_NULL(l->next);
	TEST_ASSERT_NOT_NULL(l->next->next);
	TEST_ASSERT_NULL(l->next->next->next);
	TEST_ASSERT_EQUAL_PTR(c3, l->data);
	TEST_ASSERT_EQUAL_PTR(c1, l->next->data);
	TEST_ASSERT_EQUAL_PTR(c2, l->next->next->data);
	TEST_ASSERT_TRUE(remove_contact_from_list(&l, c1));
	TEST_ASSERT_TRUE(remove_contact_from_list(&l, c2));
	TEST_ASSERT_FALSE(remove_contact_from_list(&l, c2));
	TEST_ASSERT_TRUE(remove_contact_from_list(&l, c3));
	TEST_ASSERT_NULL(l);
	TEST_ASSERT_TRUE(add_contact_to_ordered_list(&l, c2, 1));
	TEST_ASSERT_TRUE(add_contact_to_ordered_list(&l, c1, 1));
	TEST_ASSERT_TRUE(add_contact_to_ordered_list(&l, c3, 1));
	TEST_ASSERT_FALSE(add_contact_to_ordered_list(&l, c2, 1));
	TEST_ASSERT_NOT_NULL(l);
	TEST_ASSERT_NOT_NULL(l->next);
	TEST_ASSERT_NOT_NULL(l->next->next);
	TEST_ASSERT_NULL(l->next->next->next);
	TEST_ASSERT_EQUAL_PTR(c1, l->data);
	TEST_ASSERT_EQUAL_PTR(c2, l->next->data);
	TEST_ASSERT_EQUAL_PTR(c3, l->next->next->data);
	TEST_ASSERT_TRUE(remove_contact_from_list(&l, c1));
	TEST_ASSERT_TRUE(remove_contact_from_list(&l, c2));
	TEST_ASSERT_FALSE(remove_contact_from_list(&l, c2));
	TEST_ASSERT_TRUE(remove_contact_from_list(&l, c3));
	TEST_ASSERT_NULL(l);
	free_contact(c1);
	free_contact(c2);
	free_contact(c3);
}

TEST_GROUP_RUNNER(node)
{
	RUN_TEST_CASE(node, contact);
	RUN_TEST_CASE(node, endpoint_list_difference);
	RUN_TEST_CASE(node, endpoint_list_union);
	RUN_TEST_CASE(node, contact_list_union);
	RUN_TEST_CASE(node, contact_list_difference);
	RUN_TEST_CASE(node, add_contact_to_ordered_list);
}
