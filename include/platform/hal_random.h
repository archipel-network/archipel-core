/*
 * hal_random.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for random number generation functionality
 *
 */

#ifndef HAL_RANDOM_H_INCLUDED
#define HAL_RANDOM_H_INCLUDED

#include <stdint.h>

void hal_random_init(void);

uint32_t hal_random_get(void);

#endif /* HAL_RANDOM_H_INCLUDED */
