#ifndef BUNDLESTORAGEMANAGER_H_INCLUDED
#define BUNDLESTORAGEMANAGER_H_INCLUDED

#include "ud3tn/bundle.h"

#include <stdint.h>

bundleid_t bundle_storage_add(struct bundle *bundle);
int8_t bundle_storage_contains(bundleid_t id);
struct bundle *bundle_storage_get(bundleid_t id);
int8_t bundle_storage_delete(bundleid_t id);
int8_t bundle_storage_persist(bundleid_t id);
uint32_t bundle_storage_get_usage(void);

#endif /* BUNDLESTORAGEMANAGER_H_INCLUDED */
