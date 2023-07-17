// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_store.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for bundle persistance
 *
 */

#include "ud3tn/result.h"
#include "platform/hal_store.h"

enum ud3tn_result hal_store_init(void) {
    return UD3TN_OK; // TODO receive configuration of persistance location
}

enum ud3tn_result hal_store_bundle(struct bundle *bundle) {
    return UD3TN_OK; // TODO store bundle
}