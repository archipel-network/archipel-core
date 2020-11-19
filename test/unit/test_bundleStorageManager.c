#include "ud3tn/bundle.h"
#include "ud3tn/bundle_storage_manager.h"

#include "platform/hal_random.h"

#include "unity_fixture.h"

#include <stdlib.h>
#include <string.h>

TEST_GROUP(bundleStorageManager);

static const int BCNT = 32;
static const int RUNS = 30;

static struct bundle **test_bundles;
static int *test_bundle_state;

TEST_SETUP(bundleStorageManager)
{
	uint16_t i;

	test_bundles = malloc(sizeof(struct bundle *) * BCNT);
	test_bundle_state = malloc(sizeof(int) * BCNT);
	for (i = 0; i < BCNT; i++) {
		test_bundles[i] = bundle_init();
		test_bundle_state[i] = 0;
	}
}

TEST_TEAR_DOWN(bundleStorageManager)
{
	uint16_t i;

	for (i = 0; i < BCNT; i++)
		bundle_free(test_bundles[i]);
	free(test_bundles);
	free(test_bundle_state);
}

TEST(bundleStorageManager, add)
{
	uint16_t i;

	for (i = 0; i < BCNT; i++)
		TEST_ASSERT_EQUAL_INT(i + 1,
			bundle_storage_add(test_bundles[i]));
	for (i = 0; i < BCNT; i++)
		TEST_ASSERT_TRUE(bundle_storage_contains(test_bundles[i]->id));
	for (i = 0; i < BCNT; i++)
		TEST_ASSERT_EQUAL_PTR(test_bundles[i],
			bundle_storage_get(test_bundles[i]->id));
	for (i = 0; i < BCNT; i++)
		TEST_ASSERT_TRUE(bundle_storage_delete(test_bundles[i]->id));
}

/* XXX Currently unused (planned FS component) */
TEST(bundleStorageManager, add_persistent)
{
	uint16_t i;
	void *primary, *blocks;
	struct bundle *bdl;

	for (i = 0; i < BCNT; i++)
		TEST_ASSERT_EQUAL_INT(i + 1 + BCNT,
			bundle_storage_add(test_bundles[i]));
	for (i = 0; i < BCNT; i++)
		TEST_ASSERT_TRUE(bundle_storage_persist(test_bundles[i]->id));
	for (i = 0; i < BCNT; i++)
		TEST_ASSERT_TRUE(bundle_storage_contains(test_bundles[i]->id));
	for (i = 0; i < BCNT; i++) {
		bdl = bundle_storage_get(test_bundles[i]->id);
		TEST_ASSERT_NOT_NULL(bdl);
		primary = bdl->payload_block;
		blocks = bdl->blocks;
		bdl->payload_block = test_bundles[i]->payload_block;
		bdl->blocks = test_bundles[i]->blocks;
		TEST_ASSERT_FALSE(memcmp(
			test_bundles[i], bdl, sizeof(struct bundle)));
		bdl->payload_block = primary;
		bdl->blocks = blocks;
		/* XXX Has to be in persistent memory, else we get no copy! */
		bundle_free(bdl);
	}
	for (i = 0; i < BCNT; i++)
		TEST_ASSERT_TRUE(bundle_storage_delete(test_bundles[i]->id));
	for (i = 0; i < BCNT; i++)
		TEST_ASSERT_FALSE(bundle_storage_contains(test_bundles[i]->id));
}

static void toggle_bstore(int i, int persist)
{
	if (test_bundle_state[i] == 0) {
		test_bundles[i]->id = BUNDLE_INVALID_ID;
		test_bundles[i]->id = bundle_storage_add(test_bundles[i]);
		/*if (persist)*/
		/*	bundle_storage_persist(test_bundles[i]->id);*/
		test_bundle_state[i] = 1;
	} else {
		TEST_ASSERT_TRUE(bundle_storage_delete(test_bundles[i]->id));
		test_bundle_state[i] = 0;
	}
}

static void check_bstore(int i)
{
	struct bundle *bdl;
	void *primary, *blocks;

	if (test_bundle_state[i] == 0)
		return;
	TEST_ASSERT_TRUE(bundle_storage_contains(test_bundles[i]->id));
	bdl = bundle_storage_get(test_bundles[i]->id);
	TEST_ASSERT_NOT_NULL(bdl);
	if (bdl != test_bundles[i]) {
		primary = bdl->payload_block;
		blocks = bdl->blocks;
		bdl->payload_block = test_bundles[i]->payload_block;
		bdl->blocks = test_bundles[i]->blocks;
		TEST_ASSERT_FALSE(memcmp(
			test_bundles[i], bdl, sizeof(struct bundle)));
		bdl->payload_block = primary;
		bdl->blocks = blocks;
		bundle_free(bdl);
	}
}

TEST(bundleStorageManager, rand)
{
	uint16_t i, c;

	/* Test */
	for (c = 0; c < RUNS; c++) {
		for (i = 0; i < BCNT; i++) {
			if ((hal_random_get() % 2) == 0)
				toggle_bstore(i, hal_random_get() % 2);
			else
				check_bstore(i);
		}
	}
	/* Cleanup */
	for (i = 0; i < BCNT; i++) {
		if (test_bundle_state[i]) {
			TEST_ASSERT_TRUE(bundle_storage_delete(
				test_bundles[i]->id));
		}
	}
}

TEST_GROUP_RUNNER(bundleStorageManager)
{
	RUN_TEST_CASE(bundleStorageManager, add);
	/*RUN_TEST_CASE(bundleStorageManager, add_persistent);*/
	RUN_TEST_CASE(bundleStorageManager, rand);
}
