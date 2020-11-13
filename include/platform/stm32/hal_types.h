#ifndef HAL_TYPES_H_INCLUDED
#define HAL_TYPES_H_INCLUDED

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#define QueueIdentifier_t QueueHandle_t
#define Semaphore_t SemaphoreHandle_t
#define Task_t TaskHandle_t

#endif // HAL_TYPES_H_INCLUDED
