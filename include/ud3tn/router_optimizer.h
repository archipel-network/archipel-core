#ifndef ROUTEROPTIMIZER_H_INCLUDED
#define ROUTEROPTIMIZER_H_INCLUDED

#include "ud3tn/node.h"

#include "platform/hal_types.h"

Semaphore_t router_start_optimizer_task(
	QueueIdentifier_t router_signaling_queue,
	Semaphore_t clist_semaphore, struct contact_list **clistptr);

#endif /* ROUTEROPTIMIZER_H_INCLUDED */
