// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
/*
 * hal_platform.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for various hardware-specific functionality
 *
 */

#ifndef HAL_PLATFORM_H_INCLUDED
#define HAL_PLATFORM_H_INCLUDED

/**
 * @brief hal_platform_init Allows the initialization of the underlying
 *			    operating system or hardware.
 * @param argc the argument count as provided to main(...)
 * @param argv the arguments as provided to main(...)
 * Will be called once at startup
 */
void hal_platform_init(int argc, char *argv[]);

#endif /* HAL_PLATFORM_H_INCLUDED */
