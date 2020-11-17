#ifndef SIMPLEHTAB_H_INCLUDED
#define SIMPLEHTAB_H_INCLUDED

#include <stdint.h>
#include <stddef.h>

struct htab_entrylist {
	char *key;
	void *value;
	struct htab_entrylist *next;
};

struct htab {
	uint16_t slot_count;
	struct htab_entrylist **elements;
};

void htab_init(struct htab *tab, const uint16_t slot_count,
	       struct htab_entrylist *entrylist[]);
struct htab *htab_alloc(uint16_t slot_count);

void htab_trunc(struct htab *tab);
void htab_free(struct htab *tab);

struct htab_entrylist *htab_add_known(
	struct htab *tab, const char *key, const uint16_t hash,
	const size_t key_length, void *valptr,
	const uint8_t compare_ptr_only);
struct htab_entrylist *htab_add(
	struct htab *tab, const char *key, void *valptr);

void *htab_get_known(
	struct htab *tab, const char *key, const uint16_t hash,
	const uint8_t compare_ptr_only);
void *htab_get(struct htab *tab, const char *key);
struct htab_entrylist *htab_get_known_pair(
	struct htab *tab, const char *key, const uint16_t hash,
	const uint8_t compare_ptr_only);
struct htab_entrylist *htab_get_pair(struct htab *tab, const char *key);

void *htab_remove_known(
	struct htab *tab, const char *key, const uint16_t hash,
	const uint8_t compare_ptr_only);
void *htab_remove(struct htab *tab, const char *key);

#endif /* SIMPLEHTAB_H_INCLUDED */
