#include "ud3tn/simplehtab.h"

#include "unity_fixture.h"

#include <stdlib.h>
#include <stdio.h>

TEST_GROUP(simplehtab);

struct htab *htab;

TEST_SETUP(simplehtab)
{
	htab = htab_alloc(16);
}

TEST_TEAR_DOWN(simplehtab)
{
	htab_free(htab);
}

TEST(simplehtab, htab_alloc)
{
	struct htab *htab2;

	htab2 = htab_alloc(128);
	TEST_ASSERT_NULL(htab_get(htab2, "test"));
	htab_free(htab2);
}

TEST(simplehtab, htab_add)
{
	void *element = malloc(64), *get;
	struct htab_entrylist *r;

	TEST_ASSERT_NOT_NULL(element);
	r = htab_add(htab, "test", element);
	TEST_ASSERT_NOT_NULL(r);
	r = htab_add(htab, "test", element);
	TEST_ASSERT_NULL(r);
	get = htab_get(htab, "test");
	TEST_ASSERT(get == element);
	get = htab_remove(htab, "test");
	TEST_ASSERT(get == element);
	get = htab_remove(htab, "test");
	TEST_ASSERT(get == NULL);
	free(element);
}

TEST(simplehtab, htab_trunc)
{
	void *element = malloc(64), *get;
	struct htab_entrylist *r;

	TEST_ASSERT_NOT_NULL(element);
	r = htab_add(htab, "test", element);
	TEST_ASSERT_NOT_NULL(r);
	htab_trunc(htab);
	r = htab_add(htab, "test", element);
	TEST_ASSERT_NOT_NULL(r);
	get = htab_get(htab, "test");
	TEST_ASSERT(get == element);
	htab_trunc(htab);
	get = htab_get(htab, "test");
	TEST_ASSERT(get == NULL);
	free(element);
}

TEST(simplehtab, htab_add_many)
{
	const int count = 32;
	void *element = malloc(4);
	char *keys[count];
	uint16_t i;

	TEST_ASSERT_NOT_NULL(element);
	for (i = 0; i < count; i++) {
		keys[i] = malloc(5);
		snprintf(keys[i], 5, "%d", i);
		htab_add(htab, keys[i], element);
	}
	for (i = 0; i < count; i++)
		TEST_ASSERT_EQUAL_PTR(element, htab_get(htab, keys[i]));
	for (i = 0; i < count; i++) {
		htab_remove(htab, keys[i]);
		free(keys[i]);
	}
	free(element);
}

TEST_GROUP_RUNNER(simplehtab)
{
	RUN_TEST_CASE(simplehtab, htab_alloc);
	RUN_TEST_CASE(simplehtab, htab_add);
	RUN_TEST_CASE(simplehtab, htab_trunc);
	RUN_TEST_CASE(simplehtab, htab_add_many);
}
