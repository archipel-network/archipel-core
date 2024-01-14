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

struct bundle_store_popseq {
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

/**
 * @brief hal_store_popseq returns a sequence of bundles available to read
 * 
 * Returned popseq is guarendeed to return only bundles persisted before its creation
 * Ignoring newly persisted bundles (e.g. after a failed routing)
 * 
 * @param store Store to operate on (see hal_store_init)
 * @return A pop sequence iterating over available bundles or NULL if an error occured
*/
struct bundle_store_popseq* hal_store_popseq(struct bundle_store* store);

/**
 * @brief hal_store_popseq_next get next bundle in sequence
 * @param popseq Poseq to take bundle from
 * @return a bundle poped from store or NULL if there is no bundle remaining
*/
struct bundle* hal_store_popseq_next(struct bundle_store_popseq* popseq);

/**
 * @brief hal_store_popseq_free free a pop sequence created with hal_store_popseq
 * @param popseq Popseq to free
*/
void hal_store_popseq_free(struct bundle_store_popseq* popseq); 

#endif /* HAL_STORE_H_INCLUDED */
#endif /* ARCHIPEL_CORE */