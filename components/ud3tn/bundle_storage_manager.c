#include "platform/hal_semaphore.h"

#include "ud3tn/bundle.h"
#include "ud3tn/bundle_storage_manager.h"
#include "ud3tn/common.h"

#include <stdlib.h>

static struct node {
	bundleid_t id;
	struct node *successor[2];
	struct node *parent;
	struct bundle *bundle;
	int8_t height;
} *storage_tree;

static Semaphore_t tree_semaphore;

#define INV_ID BUNDLE_INVALID_ID
static bundleid_t next_id = INV_ID + 1;

static const int8_t comp_to_successor[] = {0, 0, 1};

#define max(x, y) ((x > y) ? x : y)
#define height(node) (((node) == NULL) ? 0 : (node)->height)
#define get_balance(node) (((node) == NULL) ? 0 : \
	height((node)->successor[0]) - height((node)->successor[1]))
#define recalc_height(node) ( \
	(node)->height = (1 + max( \
		height((node)->successor[0]), \
		height((node)->successor[1]))) \
)

static void lock_tree(void)
{
	if (tree_semaphore == NULL)
		tree_semaphore = hal_semaphore_init_binary();
	else
		hal_semaphore_take_blocking(tree_semaphore);
}

static void unlock_tree(void)
{
	hal_semaphore_release(tree_semaphore);
}

static struct node *create_node(
	bundleid_t id, struct bundle *bundle, struct node *parent)
{
	struct node *node = malloc(sizeof(struct node));

	if (node == NULL)
		return NULL;
	node->id = id;
	node->successor[0] = NULL;
	node->successor[1] = NULL;
	node->parent = parent;
	node->height = 1;
	node->bundle = bundle;
	return node;
}

static struct node *rotate_right(struct node *old_top)
{
	ASSERT(old_top != NULL);
	ASSERT(old_top->successor[0] != NULL);

	struct node *new_top = old_top->successor[0];
	struct node *T2 = new_top->successor[1];

	new_top->successor[1] = old_top;
	new_top->parent = old_top->parent;
	old_top->parent = new_top;
	old_top->successor[0] = T2;
	if (T2 != NULL)
		T2->parent = old_top;

	recalc_height(old_top);
	recalc_height(new_top);

	return new_top;
}

static struct node *rotate_left(struct node *old_top)
{
	ASSERT(old_top != NULL);
	ASSERT(old_top->successor[1] != NULL);

	struct node *new_top = old_top->successor[1];
	struct node *T2 = new_top->successor[0];

	new_top->successor[0] = old_top;
	new_top->parent = old_top->parent;
	old_top->parent = new_top;
	old_top->successor[1] = T2;
	if (T2 != NULL)
		T2->parent = old_top;

	recalc_height(old_top);
	recalc_height(new_top);

	return new_top;
}

/* Find a node by id, or the parent for a new node with the given id. */
static struct node **find_nodeptr(bundleid_t id, int8_t insert)
{
	struct node **prev = NULL;
	struct node **node = &storage_tree;
	int8_t comp;

	if (*node == NULL)
		return NULL;
	do {
		prev = node;
		comp = (id < (*node)->id) ? -1 : (id > (*node)->id);
		/*
		 * On insert existence
		 * must be checked beforehand
		 */
		if (comp == 0)
			return node;
		node = &(*node)->successor[comp_to_successor[comp + 1]];
	} while (*node != NULL);
	if (insert)
		return prev;
	else
		return NULL;
}

/* Gets a pointer to the nodes parent in forward traversing direction. */
/* This pointer type is needed for rebalance_from_nodeptr. */
static inline struct node **get_parent_fwd_ptr(struct node *node)
{
	if (node == NULL || node->parent == NULL)
		return NULL;
	else if (node->parent->parent == NULL)
		return &storage_tree;
	else if (node->parent == node->parent->parent->successor[0])
		return &node->parent->parent->successor[0];
	ASSERT(node->parent == node->parent->parent->successor[1]);
	return &node->parent->parent->successor[1];
}

/* XXX Important: "node" must point to a successor[] (forward) slot, */
/* not to a parent or some local pointer! */
static void rebalance_from_nodeptr(struct node **node)
{
	int8_t balance = 0;

	while (node != NULL && *node != NULL) {
		recalc_height(*node);
		balance = get_balance(*node);
		ASSERT(balance >= -2 && balance <= 2);
		if (balance == -2) { /* right is higher */
			if (get_balance((*node)->successor[1]) > 0)
				(*node)->successor[1]
					= rotate_right((*node)->successor[1]);
			*node = rotate_left(*node);
		} else if (balance == 2) { /* left is higher */
			if (get_balance((*node)->successor[0]) < 0)
				(*node)->successor[0]
					= rotate_left((*node)->successor[0]);
			*node = rotate_right(*node);
		}
		/* TODO: Might be optimizable by certain criteria ("break") */
		ASSERT(get_balance(*node) >= -1 && get_balance(*node) <= 1);
		/* node has to point to a "successor" slot */
		node = get_parent_fwd_ptr(*node);
	}
}

static void delete_node_at_ptr(struct node **delptr)
{
	struct node *del = *delptr;
	struct node *replacement;
	struct node **rebalance_from = delptr;

	/* Determine and configure replacement */
	if (del->successor[0] == NULL && del->successor[1] == NULL) {
		replacement = NULL;
	} else if (del->successor[0] != NULL && del->successor[1] == NULL) {
		replacement = del->successor[0];
	} else if (del->successor[0] == NULL && del->successor[1] != NULL) {
		replacement = del->successor[1];
	} else {
		/* Tree-node case */
		/* Prefer the higher sub-tree */
		if (height(del->successor[0]) >= height(del->successor[1])) {
			/* Determine rightmost successor of left sibling */
			replacement = del->successor[0];
			while (replacement->successor[1] != NULL)
				replacement = replacement->successor[1];
			/* Swap position in sub-tree if not direct successor */
			if (replacement != del->successor[0]) {
				replacement->parent->successor[1]
					= replacement->successor[0];
				if (replacement->successor[0] != NULL)
					replacement->successor[0]->parent
						= replacement->parent;
				recalc_height(replacement->parent);
				replacement->successor[0] = del->successor[0];
				if (del->successor[0] != NULL)
					del->successor[0]->parent = replacement;
			}
			/* "Re-hang" right successor */
			replacement->successor[1] = del->successor[1];
			if (del->successor[1] != NULL)
				del->successor[1]->parent = replacement;
			/* Rebalance from first changed node's parent */
			rebalance_from = get_parent_fwd_ptr(replacement);
			if (rebalance_from == NULL)
				rebalance_from = delptr;
		} else {
			/* Determine leftmost successor of right sibling */
			replacement = del->successor[1];
			while (replacement->successor[0] != NULL)
				replacement = replacement->successor[0];
			/* Swap position in sub-tree if not direct successor */
			if (replacement != del->successor[1]) {
				replacement->parent->successor[0]
					= replacement->successor[1];
				if (replacement->successor[1] != NULL)
					replacement->successor[1]->parent
						= replacement->parent;
				recalc_height(replacement->parent);
				replacement->successor[1] = del->successor[1];
				if (del->successor[1] != NULL)
					del->successor[1]->parent = replacement;
			}
			/* "Re-hang" left successor */
			replacement->successor[0] = del->successor[0];
			if (del->successor[0] != NULL)
				del->successor[0]->parent = replacement;
			/* Rebalance from first changed node's parent */
			rebalance_from = get_parent_fwd_ptr(replacement);
			if (rebalance_from == NULL)
				rebalance_from = delptr;
		}
	}
	/* Replace, rebalance from there or from parent if NULL and exists */
	*delptr = replacement;
	if (replacement != NULL) {
		replacement->parent = del->parent;
		rebalance_from_nodeptr(rebalance_from);
	} else if (del->parent != NULL) {
		rebalance_from_nodeptr(get_parent_fwd_ptr(del));
	}
	free(del);
}

static uint8_t add_node(bundleid_t id, struct bundle *bundle)
{
	struct node *prev = NULL;
	struct node *new = NULL;
	int8_t suc = 0;

	if (storage_tree == NULL) {
		storage_tree = create_node(id, bundle, NULL);
		return (storage_tree != NULL);
	}
	ASSERT((prev = *find_nodeptr(id, 1)) != NULL);

	new = create_node(id, bundle, prev);
	if (new == NULL)
		return 0;
	suc = comp_to_successor[1 +
		((id < prev->id) ? -1 : (id > prev->id))];
	ASSERT(prev->successor[suc] == NULL);
	prev->successor[suc] = new;

	rebalance_from_nodeptr(get_parent_fwd_ptr(new));

	return 1;
}

static uint32_t bundle_bytes;

bundleid_t bundle_storage_add(struct bundle *bundle)
{
	bundleid_t id = next_id++;

	ASSERT(bundle->id == INV_ID);
	lock_tree();
	/* TODO: This is not very efficient; */
	/* A precalculated list of free ids should be used */
	while (id == INV_ID || find_nodeptr(id, 0) != NULL)
		id = next_id++;
	if (add_node(id, bundle)) {
		bundle->id = id;
		if (bundle->payload_block)
			bundle_bytes += bundle->payload_block->length;
		/* LOGI("Stored bundle", id); */
	} else {
		id = BUNDLE_INVALID_ID;
	}
	unlock_tree();
	return id;
}

int8_t bundle_storage_contains(bundleid_t id)
{
	struct node **node;

	if (id == INV_ID)
		return 0;
	lock_tree();
	node = find_nodeptr(id, 0);
	unlock_tree();
	if (node != NULL)
		return 1;
	return 0;
}

struct bundle *bundle_storage_get(bundleid_t id)
{
	struct node **node;
	struct bundle *result = NULL;
	/*uint32_t ptr;*/

	if (id == INV_ID)
		return NULL;
	lock_tree();
	node = find_nodeptr(id, 0);
	if (node != NULL) {
		/* TODO: Persistent storage disabled */
		/*ptr = (uint32_t)(*node)->bundle;*/
		/*if ((ptr & 0x80000000) == 0x80000000)*/
		/*	result = persistent_storage_get(ptr & 0x7FFFFFFF);*/
		/*else*/
		result = (*node)->bundle;
	}
	unlock_tree();
	return result;
}

int8_t bundle_storage_delete(bundleid_t id)
{
	struct node **delptr;
	int8_t result = 0;
	/*uint32_t ptr;*/

	if (id == INV_ID)
		return 0;
	lock_tree();
	delptr = find_nodeptr(id, 0);
	if (delptr != NULL) {
		/* TODO: Persistent storage disabled */
		/*ptr = (uint32_t)(*delptr)->bundle;*/
		/*if ((ptr & 0x80000000) == 0x80000000)*/
		/*	persistent_storage_delete(ptr & 0x7FFFFFFF);*/
		const size_t size = (*delptr)->bundle->payload_block
			? (*delptr)->bundle->payload_block->length : 0;
		ASSERT(bundle_bytes >= size);
		bundle_bytes -= size;
		delete_node_at_ptr(delptr);
		result = 1;
	}
	unlock_tree();
	return result;
}

int8_t bundle_storage_persist(bundleid_t id)
{
	struct node **node;
	uint32_t ptr;
	int8_t result = 0;
	/*uint32_t pid;*/

	if (id == INV_ID)
		return 0;
	lock_tree();
	node = find_nodeptr(id, 0);
	if (node != NULL) {
		ptr = (uintptr_t)(*node)->bundle;
		/* TODO: Persistent storage disabled */
		(void)ptr;
		/*if ((ptr & 0x80000000) == 0) {*/
			/*pid = persistent_storage_add((*node)->bundle);*/
			/*if (pid == PERSISTENT_INVALID_ID) {*/
			/*	result = 0;*/
			/*} else {*/
			/*	(*node)->bundle = (void *)(pid | 0x80000000);*/
			/*	result = 1;*/
			/*}*/
		/*}*/
	}
	unlock_tree();
	return result;
}

// NOTE: The assumption is that bundles are stored in serialized form.
uint32_t bundle_storage_get_usage(void)
{
	/* TODO: Persistent storage */
	return bundle_bytes;
}

bundleid_t bundle_storage_get_next_id(void)
{
	return next_id;
}
