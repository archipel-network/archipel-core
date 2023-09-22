#ifndef HAL_STORE_H_INCLUDED
#define HAL_STORE_H_INCLUDED

#include "ud3tn/result.h"
#include "ud3tn/bundle.h"

struct bundle_store {
    const char* identifier;
};

/**
 * @brief hal_store_init initialize persistance store
 * @return Whether store was properly initialized
*/
struct bundle_store* hal_store_init(const char* identifier);

/**
 * @brief hal_store_bundle store or overwrite a bundle
 * @param bundle Bunndle to store
 * @return Whether bundle was correctly persisted
*/
enum ud3tn_result hal_store_bundle(struct bundle_store* store, struct bundle *bundle);

#endif /* HAL_STORE_H_INCLUDED */