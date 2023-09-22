#ifndef ARCHIPELC_BUNDLE_RESTORE_H
#define ARCHIPELC_BUNDLE_RESTORE_H

#include "ud3tn/result.h"
#include "platform/hal_queue.h"
#include "platform/hal_store.h"

enum bundle_restore_signal_type {
    BUNDLE_RESTORE_DEST
};

struct bundle_restore_signal {
    enum bundle_restore_signal_type type;
    char* destination;
};

struct bundle_restore_config {
    QueueIdentifier_t restore_queue;
    QueueIdentifier_t processor_signaling_queue;
    struct bundle_store* store;
};

void bundle_restore_task(void* conf);

enum ud3tn_result bundle_restore_for_destination(
    QueueIdentifier_t restore_queue,
    const char* destination
);

#endif