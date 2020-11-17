/*
 * hal_platform.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for platform-specific functionality
 *
 */

#include "platform/hal_platform.h"
#include "platform/hal_crypto.h"
#include "platform/hal_io.h"
#include "platform/hal_random.h"
#include "platform/hal_time.h"
#include "platform/hal_task.h"

#include "ud3tn/common.h"
#include "ud3tn/config.h"

#include <stm32f4xx.h>
#include <usbd_cdc_vcp.h>

#include <FreeRTOS.h>
#include <task.h>

#include <hwf4/gpio.h>
#include <hwf4/timer.h>

#include <stdlib.h>

/* Some arbitrary region numbers */
#define MPU_NULL_REGION (0x00UL << MPU_RNR_REGION_Pos)
#define MPU_CCSM_REGION (0x02UL << MPU_RNR_REGION_Pos)
#define MPU_SRAM_REGION (0x03UL << MPU_RNR_REGION_Pos)
#define MPU_FLSH_REGION (0x04UL << MPU_RNR_REGION_Pos)
#define MPU_PERI_REGION (0x05UL << MPU_RNR_REGION_Pos)
#define MPU_DWTR_REGION (0x06UL << MPU_RNR_REGION_Pos)

/* For regions, see prog. manual p. 27 (2.2 Memory model) */
/* For size values, see prog. manual p. 189f (4.2.9 MPU_RASR) => 2^(1+s) */
#define MPU_REGION_NULL_BASE (0x00000000UL) /* Flash is mapped at NULL */
#define MPU_REGION_NULL_SIZE MPU_REGION_FLSH_SIZE /* 1.0 MB */
#define MPU_REGION_CCSM_BASE CCMDATARAM_BASE
#define MPU_REGION_CCSM_SIZE (0x0FUL << MPU_RASR_SIZE_Pos) /* 64 KB */
#define MPU_REGION_SRAM_BASE SRAM_BASE
#define MPU_REGION_SRAM_SIZE (0x10UL << MPU_RASR_SIZE_Pos) /* 128 KB */
#define MPU_REGION_FLSH_BASE FLASH_BASE
#define MPU_REGION_FLSH_SIZE (0x13UL << MPU_RASR_SIZE_Pos) /* 1.0 MB */
#define MPU_REGION_PERI_BASE PERIPH_BASE
#define MPU_REGION_PERI_SIZE (0x1CUL << MPU_RASR_SIZE_Pos) /* 0.5 GB */
#define MPU_REGION_DWTR_BASE 0xE0001000UL
#define MPU_REGION_DWTR_SIZE (0x05UL << MPU_RASR_SIZE_Pos) /* 64 B */

/* RASR access flags */
#define MPU_ACCESS_NO (0x00UL << MPU_RASR_AP_Pos)
#define MPU_ACCESS_RW (0x03UL << MPU_RASR_AP_Pos)
#define MPU_ACCESS_RO (0x06UL << MPU_RASR_AP_Pos)
#define MPU_ACCESS_ROX (MPU_ACCESS_RO | MPU_RASR_XN_Msk)
#define MPU_ACCESS_RWX (MPU_ACCESS_RW | MPU_RASR_XN_Msk)
/* RASR enable flag */
#define MPU_REGION_ENABLE MPU_RASR_ENABLE_Msk


/* LED OUTPUT */

/* We define 5 LED presets for each board for debugging purposes. */
/* So we get an abstract interface that works well */
/* on both the STM32F4 discovery and SOMP2 boards. */
/* The 6th preset is for indicating errors. */
#define LED_DBG_PRESET_COUNT 6

#if defined(BOARD_STM32F4_DISCOVERY)

/* The STM32F4 discovery has 4 discretely accessible LEDs. */
#define LED_COUNT 4
#define LED_PINS { pin_d12, pin_d13, pin_d14, pin_d15 }

/* Define the association between debugging presets and LED pins: */
/* 0 .. 1 .. 2 .. 3 .. 4 LEDs on */
/* Each HEX digit represents a mode value for led_pin_set() */
/* Order is <PIN1><PIN2><PIN3><PIN4>, see LED_PINS. */
#define LED_DBG_PRESETS { 0x0000, 0x1000, 0x1100, 0x1110, 0x1111, 0x0010 }

#else

/* Fail if there is no board defined... */
#error "No known board defined!"

#endif

struct pin *leds[LED_COUNT]               = LED_PINS;
int led_dbg_presets[LED_DBG_PRESET_COUNT] = LED_DBG_PRESETS;

/* Initializes a GPIO pin */
static void led_pin_init(struct pin *p)
{
	pin_enable(p);
	pin_set_otype(p, PIN_TYPE_PUSHPULL);
	pin_set_ospeed(p, PIN_SPEED_2MHZ);
	pin_set_pupd(p, PIN_PUPD_NONE);
}

/* Initializes the defined LED pins. */
static void led_init(void)
{
	int i;

	for (i = 0; i < LED_COUNT; i++)
		led_pin_init(leds[i]);
}

/* Sets the specified led_mode value by decomposing it */
/* into HEX digits and setting the associated pins. */
/* Example: 0x102 would mean leds[0] = HI, leds[1] = LO, leds[2] = N/C */
static void led_set_mode(int led_mode)
{
	int mask = 0x3;
	int shift = 0;
	int i;

	for (i = LED_COUNT - 1; i != -1; i--) {
		hal_platform_led_pin_set(i, (led_mode & mask) >> shift);
		mask <<= 4;
		shift += 4;
	}
}



void hal_platform_led_pin_set(uint8_t led_identifier, int mode)
{
	ASSERT(led_identifier < LED_COUNT);
	if (mode == 1) { /* set */
		pin_set(leds[led_identifier]);
		pin_set_mode(leds[led_identifier], PIN_MODE_OUTPUT);
	} else if (mode == 0) { /* reset */
		pin_reset(leds[led_identifier]);
		pin_set_mode(leds[led_identifier], PIN_MODE_OUTPUT);
	} else { /* float */
		pin_set_mode(leds[led_identifier], PIN_MODE_ANALOG);
	}
}


void hal_platform_led_set(int led_preset)
{
	if (led_preset < 0 || led_preset >= LED_DBG_PRESET_COUNT)
		return;

	led_set_mode(led_dbg_presets[led_preset]);
}

void mpu_init(void)
{
	/* Disable MPU for configuration */
	MPU->CTRL &= ~MPU_CTRL_ENABLE_Msk;

	/* Region NULL: 1 MB @ 0x00000000 non-accessible */
	MPU->RNR  = MPU_NULL_REGION;
	MPU->RBAR = MPU_REGION_NULL_BASE;
	MPU->RASR = MPU_REGION_NULL_SIZE | MPU_ACCESS_NO | MPU_REGION_ENABLE;

	/* Region CCSM: CCSRAM 64 KB @ 0x10000000 RW */
	MPU->RNR  = MPU_CCSM_REGION;
	MPU->RBAR = MPU_REGION_CCSM_BASE;
	MPU->RASR = MPU_REGION_CCSM_SIZE | MPU_ACCESS_RW | MPU_REGION_ENABLE;

	/* Region SRAM: 128 KB @ 0x20000000 RW */
	MPU->RNR  = MPU_SRAM_REGION;
	MPU->RBAR = MPU_REGION_SRAM_BASE;
	MPU->RASR = MPU_REGION_SRAM_SIZE | MPU_ACCESS_RW | MPU_REGION_ENABLE;

	/* Region FLSH: FLASH 1 MB @ 0x08000000 RX */
	/* TODO: This crashess (SystemInit?) */
	/*MPU->RNR  = MPU_FLSH_REGION;
	 *MPU->RBAR = MPU_REGION_FLSH_BASE;
	 *MPU->RASR = MPU_REGION_FLSH_SIZE | MPU_ACCESS_ROX | MPU_REGION_ENABLE;
	 */

	/* Region PERI: PERIPHERAL 0.5 GB @ 0x40000000 RW */
	MPU->RNR  = MPU_PERI_REGION;
	MPU->RBAR = MPU_REGION_PERI_BASE;
	MPU->RASR = MPU_REGION_PERI_SIZE | MPU_ACCESS_RW | MPU_REGION_ENABLE;

	if (IS_DEBUG_BUILD) {
		/* Region DWTR: Data Watchpoint & Trace regs @ 0xE0001000 RW */
		MPU->RNR  = MPU_DWTR_REGION;
		MPU->RBAR = MPU_REGION_DWTR_BASE;
		MPU->RASR = (
			MPU_REGION_DWTR_SIZE | MPU_ACCESS_RW |
			MPU_REGION_ENABLE
		);
	}

	/* Enable MEMFAULT and MPU */
	SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
	MPU->CTRL |= MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
}


void hal_platform_init(int argc, char *argv[])
{
	/* Unused on STM32 */
	(void)argc;
	(void)argv;

	/* This is done by the reset handler run before main() */
	/* SystemInit(); */
	/* XXX: FreeRTOS documentation says this is important... */
	/* Ensure all priority bits are assigned as preemption priority bits. */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	led_init();
	hal_platform_led_set(0);
	timer_init();
	hal_time_init(0);
	/* Enable additional fault handlers */
	SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk;
	mpu_init();
	hal_crc_init();
	hal_hash_init();
	hal_random_init();
	hal_io_init();
}


__attribute__((noreturn))
void hal_platform_restart(void)
{
	exit(EXIT_SUCCESS);
}
