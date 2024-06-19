// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_queue.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for thread-related functionality
 *
 */

#include "platform/hal_queue.h"
#include "platform/hal_types.h"

#include "ud3tn/result.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


QueueIdentifier_t hal_queue_create(int queue_length, int item_size)
{
	QueueIdentifier_t queue = malloc(sizeof(QueueHandle_t));
	*queue = xQueueCreate(queue_length, item_size);
	return queue;
}


void hal_queue_push_to_back(QueueIdentifier_t queue, const void *item)
{
	xQueueSend(*queue, item, portMAX_DELAY);
}


enum ud3tn_result hal_queue_receive(QueueIdentifier_t queue, void *targetBuffer,
				    int64_t timeout)
{
	return xQueueReceive(*queue, targetBuffer, pdMS_TO_TICKS(timeout)) == pdTRUE
		? UD3TN_OK : UD3TN_FAIL;
}


void hal_queue_reset(QueueIdentifier_t queue)
{
	xQueueReset(*queue);
}


enum ud3tn_result hal_queue_try_push_to_back(QueueIdentifier_t queue,
					     const void *item, int64_t timeout)
{
	return xQueueSend(*queue, item, pdMS_TO_TICKS(timeout)) == pdTRUE
		? UD3TN_OK : UD3TN_FAIL;
}


void hal_queue_delete(QueueIdentifier_t queue)
{
	vQueueDelete(*queue);
	free(queue);
}


enum ud3tn_result hal_queue_override_to_back(QueueIdentifier_t queue,
					     const void *item)
{
	return xQueueOverwrite(*queue, item) == pdTRUE ? UD3TN_OK : UD3TN_FAIL;
}
