#include "platform/hal_platform.h"

#include "ud3tn/config.h"

#include <FreeRTOS.h>
#include <task.h>

#include <stm32f4xx.h>

#include <stdint.h>

/* HARDWARE FAULT HANDLERS */

/* All registers are parameters to be displayed nicely in GDB */
__attribute__((noreturn))
static void halt_fault(const char *fault, const char *task,
	const uint32_t r0, const uint32_t r1,
	const uint32_t r2, const uint32_t r3,
	const uint32_t r12, const void *lr,
	const void *pc, const uint32_t psr,
	const uint32_t scb_shcsr, const uint32_t cfsr,
	const uint32_t hfsr, const uint32_t dfsr,
	const uint32_t afsr, const uint32_t bfar)
{
	if (IS_DEBUG_BUILD)
		asm volatile ("bkpt");
	NVIC_SystemReset();
	__builtin_unreachable();
}

/* Get necessary register values from the recovered stack and halt */
static void debug_fault(const char *fault_name, const uint32_t *stack)
{
	char *task_name;

	vTaskSuspendAll();
	if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
		task_name = pcTaskGetTaskName(xTaskGetCurrentTaskHandle());
	else
		task_name = "< NO TASK >";
	halt_fault(fault_name, task_name, (uint32_t)stack[0],
		(uint32_t)stack[1], (uint32_t)stack[2],
		(uint32_t)stack[3], (uint32_t)stack[4],
		(void *)stack[5], (void *)stack[6],
		(uint32_t)stack[7], SCB->SHCSR,
		*(uint32_t *)(0xE000ED28),
		*(uint32_t *)(0xE000ED2C),
		*(uint32_t *)(0xE000ED30),
		*(uint32_t *)(0xE000ED3C),
		*(uint32_t *)(0xE000ED38));
}

__attribute__((naked))
void MemManage_Handler(void)
{
	__asm(
		" tst lr, #4\n"
		" ite eq\n"
		" mrseq r0, msp\n"
		" mrsne r0, psp\n"
		" b handle_memfault\n"
	);
}

void handle_memfault(uint32_t *stack)
{
#ifdef BOARD_STM32F4_DISCOVERY
	hal_platform_led_set(0);
	hal_platform_led_pin_set(1, 1);
#endif /* BOARD_STM32F4_DISCOVERY */
	debug_fault("MemFault", stack);
}

__attribute__((naked))
void HardFault_Handler(void)
{
	__asm(
		" tst lr, #4\n"
		" ite eq\n"
		" mrseq r0, msp\n"
		" mrsne r0, psp\n"
		" b handle_hardfault\n"
	);
}

void handle_hardfault(uint32_t *stack)
{
#ifdef BOARD_STM32F4_DISCOVERY
	hal_platform_led_set(0);
	hal_platform_led_pin_set(3, 1);
#endif /* BOARD_STM32F4_DISCOVERY */
	debug_fault("HardFault", stack);
}

__attribute__((naked))
void BusFault_Handler(void)
{
	__asm(
		" tst lr, #4\n"
		" ite eq\n"
		" mrseq r0, msp\n"
		" mrsne r0, psp\n"
		" b handle_busfault\n"
	);
}

void handle_busfault(uint32_t *stack)
{
#ifdef BOARD_STM32F4_DISCOVERY
	hal_platform_led_set(0);
	hal_platform_led_pin_set(1, 1);
	hal_platform_led_pin_set(3, 1);
#endif /* BOARD_STM32F4_DISCOVERY */
	debug_fault("BusFault", stack);
}

__attribute__((naked))
void UsageFault_Handler(void)
{
	__asm(
		" tst lr, #4\n"
		" ite eq\n"
		" mrseq r0, msp\n"
		" mrsne r0, psp\n"
		" b handle_usagefault\n"
	);
}

void handle_usagefault(uint32_t *stack)
{
#ifdef BOARD_STM32F4_DISCOVERY
	hal_platform_led_set(0);
	hal_platform_led_pin_set(1, 1);
	hal_platform_led_pin_set(2, 1);
	hal_platform_led_pin_set(3, 1);
#endif /* BOARD_STM32F4_DISCOVERY */
	debug_fault("UsageFault", stack);
}

__attribute__((naked))
void WWDG_IRQHandler(void)
{
	__asm(
		" tst lr, #4\n"
		" ite eq\n"
		" mrseq r0, msp\n"
		" mrsne r0, psp\n"
		" b handle_hardfault\n"
	);
}
