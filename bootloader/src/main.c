#include <stdint.h>

#define APP_ADDRESS 0x08008000UL

#define RCC_AHB1ENR   (*(volatile uint32_t *)0x40023830UL)
#define GPIOA_MODER   (*(volatile uint32_t *)0x40020000UL)
#define GPIOA_ODR     (*(volatile uint32_t *)0x40020014UL)

#define SCB_VTOR      (*(volatile uint32_t *)0xE000ED08UL)

typedef void (*app_entry_t)(void);

static void jump_to_application(void)
{
    uint32_t app_msp;
    uint32_t app_reset;

    /* Read application's vector table */
    app_msp   = *(volatile uint32_t *)APP_ADDRESS;
    app_reset = *(volatile uint32_t *)(APP_ADDRESS + 4U);

    /* Point interrupt vector table to application */
    SCB_VTOR = APP_ADDRESS;

    /* Set application's stack pointer */
    __asm volatile ("msr msp, %0" : : "r" (app_msp) : );

    /* Jump to application's Reset_Handler */
    ((app_entry_t)app_reset)();

    /* Should never return */
    while (1)
    {
    }
}

int main(void)
{
    /* Optional: turn LED on briefly to show bootloader ran */

    RCC_AHB1ENR |= (1U << 0);

    GPIOA_MODER &= ~(3U << (5 * 2));
    GPIOA_MODER |=  (1U << (5 * 2));

    GPIOA_ODR |= (1U << 5);

    for (volatile uint32_t i = 0; i < 10000000; i++)
    {
    }

    /* Launch application */
    jump_to_application();

    while (1)
    {
    }
}