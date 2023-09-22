#ifndef HAL_STORE_H_INCLUDED
#define HAL_STORE_H_INCLUDED

#include "ud3tn/result.h"
#include "ud3tn/bundle.h"

struct bundle_store {
    const char* identifier;
};

struct bundle_store_popseq {
    struct bundle_store* store;
    char* destination;
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
 * @brief hal_store_popseq returns a sequence of bundles available to read
 * @param store Store to operate on (see hal_store_init)
 * @param destination Node identifier of poped bundle
 * @return A pop sequence iterating over available bundles or NULL if an error occured
*/
struct bundle_store_popseq* hal_store_popseq(struct bundle_store* store, const char* destination);

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