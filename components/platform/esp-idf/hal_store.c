// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_store.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for bundle persistance
 *
 */

#include "bundle7/parser.h"
#include "bundle6/parser.h"
#include "ud3tn/result.h"
#include "platform/hal_store.h"
#include "platform/hal_io.h"
#include "platform/hal_semaphore.h"
#include <sys/stat.h>
#include "ud3tn/eid.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <inttypes.h>

#ifdef ARCHIPEL_CORE

struct bundle_store* hal_store_init(const char* identifier) {
    #warning "Store not yet implemented"
    LOGF_WARN("Bundle Store : Store not yet implemented", NULL);
    return NULL;
}

void write_bundle_to_file(void* file, const void * b, const size_t size){
    #warning "Store not yet implemented"
}

enum ud3tn_result hal_store_bundle(struct bundle_store* base_store, struct bundle *bundle) {
    #warning "Store not yet implemented"
    return UD3TN_OK;
}

struct bundle_store_popseq* hal_store_popseq(struct bundle_store* base_store){
    #warning "Store not yet implemented"
    return NULL;
}

void hal_store_popseq_free(struct bundle_store_popseq* base_popseq){
    #warning "Store not yet implemented"
}

struct bundle* hal_store_popseq_next(struct bundle_store_popseq* base_popseq){
    #warning "Store not yet implemented"
    return NULL;
}

enum ud3tn_result hal_store_set_uint64_value(
    struct bundle_store* store,
    const char* key,
    const uint64_t value){
    #warning "Store not yet implemented"
    return UD3TN_OK;
}

uint64_t hal_store_get_uint64_value(
    struct bundle_store* store,
    const char* key,
    uint64_t default_value){
    #warning "Store not yet implemented"
    return 0;
}

#endif