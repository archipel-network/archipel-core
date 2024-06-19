// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_semaphore.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for semaphore-related functionality
 *
 */

#include "platform/hal_semaphore.h"
#include "platform/esp-idf/hal_types.h"

#include "ud3tn/common.h"
#include "ud3tn/result.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

void hal_semaphore_init_inplace(Semaphore_t sem, int value)
{
	if(value == 0){
		*sem = xSemaphoreCreateBinary();
	} else {
		*sem = xSemaphoreCreateCounting(value, value);
	}
}

void hal_semaphore_take_blocking(Semaphore_t sem)
{
	int ret;

	do {
		ret = xSemaphoreTake(*sem, portMAX_DELAY);
	} while (ret == pdFALSE);
}

void hal_semaphore_release(Semaphore_t sem)
{
	xSemaphoreGive(*sem);
}

bool hal_semaphore_is_blocked(Semaphore_t sem)
{
	int rv = uxSemaphoreGetCount(*sem);

	return rv == 0;
}

void hal_semaphore_delete(Semaphore_t sem)
{
	vSemaphoreDelete(*sem);
	free(sem);
}

enum ud3tn_result hal_semaphore_try_take(Semaphore_t sem, int64_t timeout_ms)
{
	int ret = xSemaphoreTake(*sem, pdMS_TO_TICKS(timeout_ms));
	return ret == pdFALSE ? UD3TN_FAIL : UD3TN_OK;
}

// Functions not specific to whether it is an Apple or non-Apple platform

Semaphore_t hal_semaphore_init_binary(void)
{
	Semaphore_t sem = malloc(sizeof(SemaphoreHandle_t));

	if (sem)
		hal_semaphore_init_inplace(sem, 0);
	return sem;
}

Semaphore_t hal_semaphore_init_value(int value)
{
	Semaphore_t sem = malloc(sizeof(SemaphoreHandle_t));

	if (sem)
		hal_semaphore_init_inplace(sem, value);
	return sem;
}
