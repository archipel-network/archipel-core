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

/**
 * @brief hal_semaphoreInitBinary Creates a new binary semaphore meaning that
 *				  only one execution entity can hold at a
 *				  specific moment
 * @return An OS-specific identifier for the created semaphore
 */
Semaphore_t hal_semaphore_init_binary(void);

/**
 * @brief hal_semaphoreTake Take a previously initialized semaphore, if the
 *			    semaphore is alread taken, block until it is
 *			    released
 * @param sem The identifier of the semaphore that should be locked
 */
void hal_semaphore_take_blocking(Semaphore_t sem);

/**
 * @brief hal_semaphoreTake Poll the semaphore, e.g. check whether it is
 *			    available at the specific moment
 * @param sem The identifier of the semaphore that should be polled
 */
void hal_semaphore_poll(Semaphore_t sem);

/**
 * @brief hal_semaphoreRelease Release a previously initialized and taken
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
 * @param sem The semaphore that should be locket
 * @param timeout_ms The timeout in Milliseconds
 * @return Whether the operation was successful
 */
enum ud3tn_result hal_semaphore_try_take(Semaphore_t sem, int timeout_ms);


#endif /* HAL_SEMAPHORE_H_INCLUDED */
