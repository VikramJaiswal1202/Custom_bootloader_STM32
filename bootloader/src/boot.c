
#include <stdint.h>
#include "boot.h"

#define APP_START_ADDRESS  0x08008000UL
#define APP_FLASH_END      0x08080000UL

#define SRAM_START         0x20000000UL
#define SRAM_END           0x20020000UL

#define SCB_VTOR            (*(volatile uint32_t *)0xE000ED08UL)
#define SCB_ICSR            (*(volatile uint32_t *)0xE000ED04UL)

#define SCB_ICSR_PENDSTCLR  (1UL << 25)
#define SCB_ICSR_PENDSVCLR  (1UL << 27)

#define NVIC_ICER_BASE      ((volatile uint32_t *)0xE000E180UL)
#define NVIC_ICPR_BASE      ((volatile uint32_t *)0xE000E280UL)
#define NVIC_REGISTER_COUNT 8U

typedef void (*application_entry_t)(void);

int boot_validate_application(void)
{
    uint32_t app_msp =
        *(volatile const uint32_t *)APP_START_ADDRESS;

    uint32_t app_reset =
        *(volatile const uint32_t *)(APP_START_ADDRESS + 4U);

    if (app_msp < SRAM_START ||
        app_msp > SRAM_END ||
        (app_msp & 0x7U) != 0U)
    {
        return -1;
    }

    if ((app_reset & 1U) == 0U)
    {
        return -1;
    }

    uint32_t reset_address = app_reset & ~1UL;

    if (reset_address < APP_START_ADDRESS ||
        reset_address >= APP_FLASH_END)
    {
        return -1;
    }

    return 0;
}

void boot_jump_to_application(void)
{
    uint32_t app_msp;
    uint32_t app_reset;
    application_entry_t entry;

    if (boot_validate_application() != 0)
    {
        return;
    }

    app_msp = *(volatile const uint32_t *)APP_START_ADDRESS;
    app_reset = *(volatile const uint32_t *)(APP_START_ADDRESS + 4U);
    entry = (application_entry_t)app_reset;

    /* Disable interrupts during the handoff. */
    __asm volatile ("cpsid i" ::: "memory");

    /* Stop SysTick and clear its pending exception state. */
    *(volatile uint32_t *)0xE000E010UL = 0U;
    *(volatile uint32_t *)0xE000E014UL = 0U;
    *(volatile uint32_t *)0xE000E018UL = 0U;

    SCB_ICSR = SCB_ICSR_PENDSTCLR | SCB_ICSR_PENDSVCLR;

    /* Disable and clear all external NVIC interrupt lines. */
    for (uint32_t i = 0U; i < NVIC_REGISTER_COUNT; i++)
    {
        NVIC_ICER_BASE[i] = 0xFFFFFFFFUL;
        NVIC_ICPR_BASE[i] = 0xFFFFFFFFUL;
    }

    /* Point exception vectors at the application. */
    SCB_VTOR = APP_START_ADDRESS;

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    /*
     * Set the application's stack pointer and branch to Reset_Handler.
     * Keep interrupts disabled during the transition.
     */
    __asm volatile (
        "msr msp, %0 \n"
        "bx %1       \n"
        :
        : "r" (app_msp), "r" (entry)
        : "memory"
    );

    __builtin_unreachable();
}
