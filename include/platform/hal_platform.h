/*
 * hal_platform.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for various hardware-specific functionality
 *
 */

#ifndef HAL_PLATFORM_H_INCLUDED
#define HAL_PLATFORM_H_INCLUDED

#include <stdint.h>

/**
 * @brief hal_platform_led_pin_set Sets the specified output mode for the
 *				   specified GPIO pin.
 * @param p The GPIO pin that should be set
 * @param mode The target mode: mode = 0: LOW
 *				mode = 1: HIGH
 *				mode = 2: FLOAT ("analog")
 */
void hal_platform_led_pin_set(uint8_t led_identifier, int mode);

/**
 * @brief hal_platform_led_set Sets the specified led_mode value by
 *			       decomposing it into HEX digits and setting
 *			       the associated pins
 * @code{.c}
 *	Example: 0x102 would mean leds[0] = HI, leds[1] = LO, leds[2] = N/C
 * @endcode
 * @param led_preset An numeric value representing the whole RGB range
 */
void hal_platform_led_set(int led_preset);

/**
 * @brief hal_platform_init Allows the initialization of the underlying
 *			    operating system or hardware.
 * @param argc the argument count as provided to main(...)
 * @param argv the arguments as provided to main(...)
 * Will be called once at startup
 */
void hal_platform_init(int argc, char *argv[]);

/**
 * @brief hal_platform_restart Restarts the application (either by
 *			       restarting the whole system or just the app)
 */
__attribute__((noreturn))
void hal_platform_restart(void);

#endif /* HAL_PLATFORM_H_INCLUDED */
