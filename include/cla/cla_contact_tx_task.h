#ifndef CLA_CONTACT_TX_TASK_H_INCLUDED
#define CLA_CONTACT_TX_TASK_H_INCLUDED

#include "cla/cla.h"

#include "ud3tn/node.h"
#include "ud3tn/router.h"

#include "platform/hal_queue.h"

enum cla_contact_tx_task_command_type {
	TX_COMMAND_UNDEFINED, /* 0x00 */
	TX_COMMAND_BUNDLES,   /* 0x01 */
	TX_COMMAND_FINALIZE,  /* 0x02 */
};

struct cla_contact_tx_task_command {
	enum cla_contact_tx_task_command_type type;
	struct routed_bundle_list *bundles;
};

enum ud3tn_result cla_launch_contact_tx_task(struct cla_link *link);

void cla_contact_tx_task_request_exit(QueueIdentifier_t queue);

#endif /* CLA_CONTACT_TX_TASK_H_INCLUDED */
