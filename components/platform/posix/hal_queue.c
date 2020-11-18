/*
 * hal_queue.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for thread-related functionality
 *
 */

#include "platform/hal_queue.h"
#include "platform/hal_types.h"

#include "platform/posix/simple_queue.h"

#include "ud3tn/result.h"

#include <stdlib.h>
#include <stdio.h>


QueueIdentifier_t hal_queue_create(int queue_length, int item_size)
{
	return queueCreate(queue_length, item_size);
}


void hal_queue_push_to_back(QueueIdentifier_t queue, const void *item)
{
	queuePush(queue, item, -1, false);
}


enum ud3tn_result hal_queue_receive(QueueIdentifier_t queue, void *targetBuffer,
				    int timeout)
{
	return queuePop(queue, targetBuffer, timeout) == 0
		? UD3TN_OK : UD3TN_FAIL;
}


void hal_queue_reset(QueueIdentifier_t queue)
{
	queueReset(queue);
}


enum ud3tn_result hal_queue_try_push_to_back(QueueIdentifier_t queue,
					     const void *item, int timeout)
{
	return queuePush(queue, item, timeout, false) == 0
		? UD3TN_OK : UD3TN_FAIL;
}


void hal_queue_delete(QueueIdentifier_t queue)
{
	queueDelete(queue);
}


enum ud3tn_result hal_queue_override_to_back(QueueIdentifier_t queue,
					     const void *item)
{
	return queuePush(queue, item, -1, true) == 0 ? UD3TN_OK : UD3TN_FAIL;
}

uint8_t hal_queue_nr_of_items_waiting(QueueIdentifier_t queue)
{
	return queueItemsWaiting(queue);
}
