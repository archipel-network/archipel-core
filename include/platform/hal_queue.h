/*
 * hal_queue.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for queue-related functionality
 *
 */

#ifndef HAL_QUEUE_H_INCLUDED
#define HAL_QUEUE_H_INCLUDED

#include "platform/hal_types.h"

#include "ud3tn/result.h"

#include <stdint.h>
#include <stddef.h>

/**
 * @brief hal_queue_create Creates a new channel for inter-task communication
 * @param queueLength The maximum number of items than can be stored inside
 *                    the queue
 * @param itemSize The size of one item in bytes
 * @return A queue identifier
 */
QueueIdentifier_t hal_queue_create(int queue_length, int item_size);


/**
 * @brief hal_queue_delete Deletes a specified queue and frees its memory
 * @param queue The queue that should be deleted
 */
void hal_queue_delete(QueueIdentifier_t queue);

/**
 * @brief hal_queue_push_to_back Attach a given item to the back of the queue.
 *                            Has blocking behaviour, i.e. tries to insert
 *                            the element in the underlying OS structure
 *                            indefinitely
 * @param queue The identifier of the Queue that the element should be
 *              inserted
 * @param item  The target item
 */
void hal_queue_push_to_back(QueueIdentifier_t queue, const void *item);

/**
 * @brief hal_queue_try_push_to_back Attach a given item to the back of the
 *			      queue.
 *                            Has blocking behaviour, i.e. tries to insert
 *                            the element in the underlying OS structure
 *                            indefinitely
 * @param queue The identifier of the Queue that the element should be
 *              inserted
 * @param item  The target item
 * @param timeout After which time (in milliseconds) the receiving attempt
 *		  should be aborted.
 *		  If this value is -1, receiving will block indefinitely
 * @return Whether the attachment attempt was successful
 */
enum ud3tn_result hal_queue_try_push_to_back(QueueIdentifier_t queue,
					     const void *item,
					     int timeout);

/**
 * @brief hal_queue_override_to_back Attach a given item to the back of the
 *			      queue. If there is no space available, override
 *			      previous elements
 * @param queue The identifier of the Queue that the element should be
 *              inserted
 * @param item  The target item
 * @return Whether the attachment attempt was successful --> that is per
 *	   functionality of this function always true!
 */
enum ud3tn_result hal_queue_override_to_back(QueueIdentifier_t queue,
					     const void *item);

/**
 * @brief hal_queue_receive Receive a item from the specific queue
 *			    Has blocking behaviour!
 * @param queue The identifier of the Queue that the element should be read
 *		from
 * @param targetBuffer A pointer to the memory where the received item should
 *		       be stored
 * @param timeout After which time (in milliseconds) the receiving attempt
 *		  should be aborted.
 *		  If this value is -1, receiving will block indefinitely
 * @return Whether the receiving was successful
 */
enum ud3tn_result hal_queue_receive(QueueIdentifier_t queue,
				    void *targetBuffer,
				    int timeout);

/**
 * @brief hal_queue_reset Reset (i.e. empty) the specific queue
 * @param queue The queue that should be cleared
 */
void hal_queue_reset(QueueIdentifier_t queue);

/**
 * @brief hal_queue_nr_of_data_waiting Return the number of waiting items
 *				       in a queue
 * @param The queue that should be checked
 * @return 0 if the queue doesn't exist, otherwise the number of waiting
 *	     items
 */
uint8_t hal_queue_nr_of_items_waiting(QueueIdentifier_t queue);

#endif /* HAL_QUEUE_H_INCLUDED */
