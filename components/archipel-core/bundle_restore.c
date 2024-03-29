#ifdef ARCHIPEL_CORE
#include "archipel-core/bundle_restore.h"
#include "platform/hal_queue.h"
#include "platform/hal_io.h"
#include "ud3tn/bundle_processor.h"
#include <stdlib.h>
#include <string.h>

void bundle_restore_task(void* conf){
    struct bundle_restore_config* config = 
        (struct bundle_restore_config*) conf;

    struct bundle_restore_signal signal;
    enum ud3tn_result result;
    LOG_INFO("BundleRestore : Bundle restore task started");
    for (;;)
    {
        result = hal_queue_receive(config->restore_queue, &signal, -1);
        if(result == UD3TN_FAIL){
            LOG_ERROR("BundleRestore : Error receiving message on queue");
            continue;
        }
        if(signal.type == BUNDLE_RESTORE_DEST){
            LOGF_INFO("BundleRestore : Should restore for %s", signal.destination);

            struct bundle_store_popseq* seq = 
                hal_store_popseq(config->store);

            struct bundle* bundle = NULL;
            while((bundle = hal_store_popseq_next(seq)) != NULL){
                bundle_processor_inform(
                    config->processor_signaling_queue,
					(struct bundle_processor_signal) {
						.type = BP_SIGNAL_BUNDLE_INCOMING,
						.bundle = bundle
                    }
                );
            }

            hal_store_popseq_free(seq);
            free(signal.destination);
        }
    }

    ASSERT(0);
}

enum ud3tn_result bundle_restore_for_destination(
    QueueIdentifier_t restore_queue,
    const char* destination
){
    struct bundle_restore_signal signal = (struct bundle_restore_signal) { 
        .type = BUNDLE_RESTORE_DEST,
        .destination = strdup(destination)
    };
    return hal_queue_try_push_to_back(restore_queue, &signal, -1);
}
#endif