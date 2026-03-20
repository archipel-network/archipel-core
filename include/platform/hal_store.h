#ifdef ARCHIPEL_CORE
#ifndef HAL_STORE_H_INCLUDED
#define HAL_STORE_H_INCLUDED

#define DEFAULT_STORE_LOCATION "archipel-core-bundles"
#define HAL_STORE_READ_BUFFER_SIZE 2048

#include "ud3tn/result.h"
#include "ud3tn/bundle.h"

struct bundle_store {
    const char* identifier;
};

struct bundle_store_loadall {
    struct bundle_store* store;
};

/**
 * @brief hal_store_init initialize persistance store
 * @return Whether store was properly initialized
*/
struct bundle_store* hal_store_init(const char* identifier);

/**
 * @brief hal_store_bundle persists a bundle
 * @param store Store to operate on (see hal_store_init)
 * @param bundle Bundle to persist
 * @return Whether bundle was correctly persisted
*/
enum ud3tn_result hal_store_bundle(struct bundle_store* store, struct bundle *bundle);

// TODO Doc
enum ud3tn_result hal_store_bundle_metadata(struct bundle_store* store, struct bundle *bundle);

// TODO Doc
enum ud3tn_result hal_store_bundle_delete(struct bundle_store* store, struct bundle *bundle);

/**
 * @brief hal_store_set_uint64_value store a value identified by a key
 * @param store Store to put value in
 * @param key key identifying value
 * @param value Value to store
 * @return UD3TN_FAIL if operation failed UD3TN_OK otherwise
*/
enum ud3tn_result hal_store_set_uint64_value(struct bundle_store* store, const char* key, const uint64_t value);

/**
 * @brief hal_store_get_uint64_value retreive a value from store identifier by a key
 * @param store Store to get value from
 * @param key Key of value to retreive
 * @param default_value If value if missing return default value
 * @return current value
*/
uint64_t hal_store_get_uint64_value(struct bundle_store* store, const char* key, uint64_t default_value);

// TODO Doc
struct bundle_store_loadall* hal_store_loadall(struct bundle_store* store);

// TODO Doc
struct bundle* hal_store_loadall_next(struct bundle_store_loadall* loader);

// TODO Doc
void hal_store_loadall_free(struct bundle_store_loadall* loader); 

#endif /* HAL_STORE_H_INCLUDED */
#endif /* ARCHIPEL_CORE */