#include "ud3tn/common.h"
#include "ud3tn/simplehtab.h"

#include "util/htab_hash.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define HASHL(key, len) (hashlittle(key, len, 0))
/* Gets length of string, should be optimized out... */
#define HASH(key) (HASHL(key, strlen(key)))

void htab_init(struct htab *tab, const uint16_t slot_count,
	       struct htab_entrylist *entrylist[])
{
	int i;

	ASSERT(tab != NULL);
	tab->slot_count = slot_count;
	tab->elements = entrylist;
	for (i = 0; i < tab->slot_count; i++)
		tab->elements[i] = NULL;
}

struct htab *htab_alloc(const uint16_t slot_count)
{
	int i;
	struct htab *tab = (struct htab *)malloc(sizeof(struct htab));

	ASSERT(slot_count != 0);
	tab->slot_count = slot_count;
	tab->elements = (struct htab_entrylist **)
		malloc(sizeof(struct htab_entrylist *) * slot_count);
	for (i = 0; i < slot_count; i++)
		tab->elements[i] = NULL;

	return tab;
}

void htab_free(struct htab *tab)
{
	htab_trunc(tab);
	free(tab->elements);
	free(tab);
}

void htab_trunc(struct htab *tab)
{
	int i;
	struct htab_entrylist *cur, *next;

	ASSERT(tab != NULL);
	ASSERT(tab->elements != NULL);
	for (i = 0; i < tab->slot_count; i++) {
		cur = tab->elements[i];
		while (cur != NULL) {
			free(cur->key);
			next = cur->next;
			free(cur);
			cur = next;
		}
		tab->elements[i] = NULL;
	}
}

static struct htab_entrylist **get_elist_ptr_by_hash(
	struct htab *tab, uint16_t hash, const char *key,
	const uint8_t compare_ptr_only)
{
	struct htab_entrylist **cur_elem;

	ASSERT(tab->elements != NULL);
	cur_elem = &(tab->elements[hash]);
	if (compare_ptr_only) {
		while (*cur_elem != NULL) {
			if ((*cur_elem)->key == key)
				break;
			cur_elem = &((*cur_elem)->next);
		}
	} else {
		while (*cur_elem != NULL) {
			if ((*cur_elem)->key == key
					|| strcmp((*cur_elem)->key, key) == 0)
				break;
			cur_elem = &((*cur_elem)->next);
		}
	}

	if (*cur_elem == NULL)
		return NULL;
	else
		return cur_elem;
}

struct htab_entrylist *htab_add_known(
	struct htab *tab, const char *key, const uint16_t hash,
	const size_t key_length, void *valptr,
	const uint8_t compare_ptr_only)
{
	struct htab_entrylist *new_elem, **cur_elem;
	uint16_t shash;

	ASSERT(tab != NULL);
	ASSERT(tab->slot_count != 0);
	shash = hash % tab->slot_count;
	if (get_elist_ptr_by_hash(tab, shash, key, compare_ptr_only) != NULL)
		return NULL;

	new_elem = malloc(sizeof(struct htab_entrylist));
	new_elem->key = malloc(key_length + 1);
	strncpy(new_elem->key, key, key_length + 1);
	new_elem->value = valptr;
	new_elem->next = NULL;

	cur_elem = &(tab->elements[shash]);
	while (*cur_elem != NULL)
		cur_elem = &((*cur_elem)->next);

	*cur_elem = new_elem;

	return new_elem;
}

struct htab_entrylist *htab_add(
	struct htab *tab, const char *key, void *valptr)
{
	size_t key_length = strlen(key);

	return htab_add_known(
		tab, key, HASHL(key, key_length), key_length, valptr, 0);
}

void *htab_get_known(
	struct htab *tab, const char *key, const uint16_t hash,
	const uint8_t compare_ptr_only)
{
	struct htab_entrylist **elist_ptr;

	ASSERT(tab != NULL);
	ASSERT(tab->slot_count != 0);
	elist_ptr = get_elist_ptr_by_hash(
		tab, hash % tab->slot_count, key, compare_ptr_only);
	if (elist_ptr == NULL)
		return NULL;
	else
		return (*elist_ptr)->value;
}

void *htab_get(struct htab *tab, const char *key)
{
	return htab_get_known(tab, key, HASH(key), 0);
}

struct htab_entrylist *htab_get_known_pair(
	struct htab *tab, const char *key, const uint16_t hash,
	const uint8_t compare_ptr_only)
{
	struct htab_entrylist **elist_ptr;

	ASSERT(tab != NULL);
	ASSERT(tab->slot_count != 0);
	elist_ptr = get_elist_ptr_by_hash(
		tab, hash % tab->slot_count, key, compare_ptr_only);
	if (elist_ptr == NULL)
		return NULL;
	else
		return *elist_ptr;
}

struct htab_entrylist *htab_get_pair(struct htab *tab, const char *key)
{
	return htab_get_known_pair(tab, key, HASH(key), 0);
}

void *htab_remove_known(
	struct htab *tab, const char *key, const uint16_t hash,
	const uint8_t compare_ptr_only)
{
	struct htab_entrylist **elist_ptr, *next_ptr;
	void *valptr;

	ASSERT(tab != NULL);
	ASSERT(tab->slot_count != 0);
	elist_ptr = get_elist_ptr_by_hash(
		tab, hash % tab->slot_count, key, compare_ptr_only);
	if (elist_ptr == NULL)
		return NULL;

	valptr = (*elist_ptr)->value;
	next_ptr = (*elist_ptr)->next;

	/* Free memory */
	free((*elist_ptr)->key);
	free(*elist_ptr);

	/* Set current list element to next element (or NULL) */
	*elist_ptr = next_ptr;

	return valptr;
}

void *htab_remove(struct htab *tab, const char *key)
{
	return htab_remove_known(tab, key, HASH(key), 0);
}
