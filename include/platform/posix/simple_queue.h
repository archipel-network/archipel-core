/*
 * simple_queue.h
 *
 * Description: simple and lightweight implementation of message-queues in C.
 *
 * Copyright (c) 2016, Robert Wiewel
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the  nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SIMPLE_QUEUE_H_INCLUDED
#define SIMPLE_QUEUE_H_INCLUDED

#include <semaphore.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief The Queue_t struct Data structure holding all queue-related info
 */
typedef struct {
	// absolut length of the queue (in items)
	int item_length;
	// size of one single item (in bytes)
	int item_size;

	// provides mutual exclusion for the given queue
	sem_t semaphore;

	// these semaphores circumvent busy waiting
	sem_t sem_pop;
	sem_t sem_push;

	// abs_start and abs_end are pointers to the first and last
	// byte of the reserved memory
	char *abs_start;
	char *abs_end;

	// current_start and current_end point to the first and last currently
	// used elements in the queue
	char *current_start;
	char *current_end;

} Queue_t;

/**
 * @brief queueCreate Creates a new queue structure, i.e. allocates the
 *			necessary memory ressources
 * @param queue_length absolute length of the queue (in items)
 * @param item_size size of a single item (in bytes)
 * @return returns a pointer to the queue "object"
 */
Queue_t *queueCreate(unsigned int queue_length, unsigned int item_size);

/**
 * @brief queueDelete Delete a queue structure, i.e. frees the memory
 *			ressources
 * @param queue The pointer to the queue structure
 */
void queueDelete(Queue_t *queue);

/**
 * @brief queueReset Reset a given queue, i.e. empty the data section
 * @param queue The pointer to the queue structure
 */
void queueReset(Queue_t *queue);

/**
 * @brief queueItemsWaiting Returns the number of items that are currently in
 *				the list
 * @param queue	The pointer to the queue structure
 * @return The number of items in the list
 */
unsigned int queueItemsWaiting(Queue_t *queue);

/**
 * @brief queuePush Pushes an item to the end of the queue
 * @param queue The pointer to the queue structure
 * @param item A pointer to the item that should be queued
 * @param timeout Defines how long the queuing should be tried
 *			(in milliseconds)
 * @param force Defines if the last element of the queue should be replaced
 *		forcefully (only applied when queue is full)
 * @return Exitcode, if queueing was successfull
 */
uint8_t queuePush(Queue_t *queue, const void *item, int timeout, bool force);

/**
 * @brief queuePop Pops the upmost element of the queue
 * @param queue The pointer to the queue structure
 * @param targetBuffer The memory location where the queued item should be
 *			copied to
 * @param timeout Defines how long the dequeuing should be tried
 *			(in milliseconds)
 * @return Exitcode, if dequeueing was successfull
 */
uint8_t queuePop(Queue_t *queue, void *targetBuffer, int timeout);

#endif /* SIMPLE_QUEUE_H_INCLUDED */
