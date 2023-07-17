#ifndef HAL_STORE_H_INCLUDED
#define HAL_STORE_H_INCLUDED

#include "ud3tn/result.h"
#include "ud3tn/bundle.h"

/**
 * @brief hal_store_init initialize persistance store
 * @return Whether store was properly initialized
*/
enum ud3tn_result hal_store_init(void);

/**
 * @brief hal_store_bundle store or overwrite a bundle
 * @param bundle Bunndle to store
 * @return Whether bundle was correctly persisted
*/
enum ud3tn_result hal_store_bundle(struct bundle *bundle);

#endif /* HAL_STORE_H_INCLUDED */