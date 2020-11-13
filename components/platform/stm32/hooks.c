// Implementation of FreeRTOS hooks.
// See: https://www.freertos.org/a00016.html

#include <FreeRTOS.h>
#include <task.h>

#include <stm32f4xx.h>

#include <stdlib.h>

void vApplicationTickHook(void)
{
}

void vApplicationIdleHook(void)
{
}

__attribute__((noreturn))
void vApplicationMallocFailedHook(void)
{
	exit(EXIT_FAILURE);
}

__attribute__((noreturn))
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
	(void)pcTaskName;
	(void)pxTask;
	exit(EXIT_FAILURE);
}
