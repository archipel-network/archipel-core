/*
 * hal_random.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for random number gerneration functionality
 *
 */

#include "platform/hal_random.h"

#include <FreeRTOS.h>
#include <stm32f4xx.h>


void hal_random_init(void)
{
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
	RNG_Cmd(ENABLE);
}


uint32_t hal_random_get(void)
{
	while (RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET)
		;

	return RNG_GetRandomNumber();
}
