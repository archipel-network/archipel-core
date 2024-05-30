#ifndef HAL_TYPES_H_INCLUDED
#define HAL_TYPES_H_INCLUDED

#include <freertos/FreeRTOS.h>
#include <sys/types.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#define Semaphore_t SemaphoreHandle_t
#define QueueIdentifier_t QueueHandle_t
#define HAL_SEMAPHORE_MAX_DELAY_MS 9223372036854ULL
#define HAL_QUEUE_MAX_DELAY_MS HAL_SEMAPHORE_MAX_DELAY_MS

#endif