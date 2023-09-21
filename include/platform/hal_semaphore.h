// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_semaphore.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for semaphore-related functionality
 *
 */

#ifndef HAL_SEMAPHORE_H_INCLUDED
#define HAL_SEMAPHORE_H_INCLUDED

#include "platform/hal_types.h"

#include "ud3tn/result.h"

#include <stdint.h>

/**
 * @brief hal_semaphore_init_binary Creates a new binary semaphore meaning that
 *				  only one execution entity can hold at a
 *				  specific moment
 * @return An OS-specific identifier for the created semaphore
 */
Semaphore_t hal_semaphore_init_binary(void);

/**
 * @brief hal_semaphore_init_value Creates a new semaphore with the given
 *				  initial value
 * @return An OS-specific identifier for the created semaphore
 */
Semaphore_t hal_semaphore_init_value(int value);

/**
 * @brief hal_semaphore_take_blocking Take a previously initialized semaphore,
 *			    if the semaphore is alread taken, block until it is
 *			    released
 * @param sem The identifier of the semaphore that should be locked
 */
void hal_semaphore_take_blocking(Semaphore_t sem);

/**
 * @brief hal_semaphore_is_blocked Poll the semaphore, e.g. check whether it is
 *			    available at the specific moment
 * @param sem The identifier of the semaphore that should be polled
 */
bool hal_semaphore_is_blocked(Semaphore_t sem);

/**
 * @brief hal_semaphore_release Release a previously initialized and taken
 *			       semaphore
 * @param sem The identifier of the semaphore that should be freed
 */
void hal_semaphore_release(Semaphore_t sem);

/**
 * @brief hal_semaphore_delete Delete a semaphore
 * @param sem The identifier of the semaphore that should be deleted
 */
void hal_semaphore_delete(Semaphore_t sem);


/**
 * @brief hal_semaphore_try_take Try to take a initialized semaphore, abort
 *				 after the timeout is reached and return
 *				 an error
 * @param sem The semaphore that should be locked
 * @param timeout_ms The timeout in Milliseconds, in the range of 0 to
 *		     9223372036854. If outside of this range, the function is
 *		     equivalent to hal_semaphore_take_blocking
 * @return Whether the operation was successful; UD3TN_FAIL on timeout
 */
enum ud3tn_result hal_semaphore_try_take(Semaphore_t sem, int64_t timeout_ms);


#endif /* HAL_SEMAPHORE_H_INCLUDED */
