#ifndef CONTACTMANAGER_H_INCLUDED
#define CONTACTMANAGER_H_INCLUDED

#include "ud3tn/node.h"

#include "platform/hal_types.h"

#include <stdint.h>

struct contact_manager_params {
	Semaphore_t semaphore;
	QueueIdentifier_t control_queue;
};

/* Flags what should be checked */
enum contact_manager_signal {
	CM_SIGNAL_NONE = 0x0,
	CM_SIGNAL_UPDATE_CONTACT_LIST = 0x1,
	CM_SIGNAL_PROCESS_CURRENT_BUNDLES = 0x2,
	CM_SIGNAL_UNKNOWN = 0x3
};

struct contact_manager_params contact_manager_start(
	QueueIdentifier_t router_signaling_queue,
	struct contact_list **clistptr);

uint64_t contact_manager_get_next_contact_time(void);
uint8_t contact_manager_in_contact(void);
void contact_manager_reset_time(void);

#endif /* CONTACTMANAGER_H_INCLUDED */
