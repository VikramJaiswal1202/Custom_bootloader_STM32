#include <stdint.h>

#define APP_ADDRESS  0x08008000UL

#define SRAM_START   0x20000000UL
#define SRAM_END     0x20020000UL

#define FLASH_START  0x08008000UL
#define FLASH_END    0x08080000UL

#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830UL)
#define GPIOA_MODER  (*(volatile uint32_t *)0x40020000UL)
#define GPIOA_ODR    (*(volatile uint32_t *)0x40020014UL)

#define SCB_VTOR     (*(volatile uint32_t *)0xE000ED08UL)

typedef void (*app_entry_t)(void);


/*----------------------------------------------------------
 * Check application's initial MSP
 *----------------------------------------------------------*/
static int is_valid_msp(uint32_t msp)
{
    return (msp >= SRAM_START) && (msp <= SRAM_END);
}


/*----------------------------------------------------------
 * Check application's Reset_Handler
 *----------------------------------------------------------*/
static int is_valid_reset_handler(uint32_t reset_handler)
{
    /* Must be inside application Flash */
    if (reset_handler < FLASH_START ||
        reset_handler >= FLASH_END)
    {
        return 0;
    }

    /* Cortex-M4 code must have Thumb bit set */
    if ((reset_handler & 1U) == 0)
    {
        return 0;
    }

    return 1;
}


/*----------------------------------------------------------
 * Check whether a valid application exists
 *----------------------------------------------------------*/
static int is_application_valid(void)
{
    uint32_t app_msp;
    uint32_t app_reset;

    /* Read application's vector table */
    app_msp   = *(volatile uint32_t *)APP_ADDRESS;
    app_reset = *(volatile uint32_t *)(APP_ADDRESS + 4U);

    if (!is_valid_msp(app_msp))
    {
        return 0;
    }

    if (!is_valid_reset_handler(app_reset))
    {
        return 0;
    }

    return 1;
}


/*----------------------------------------------------------
 * Jump to application
 *----------------------------------------------------------*/
static void jump_to_application(void)
{
    uint32_t app_msp;
    uint32_t app_reset;

    /* Read application's vector table */
    app_msp   = *(volatile uint32_t *)APP_ADDRESS;
    app_reset = *(volatile uint32_t *)(APP_ADDRESS + 4U);

    /* Move interrupt vector table to application */
    SCB_VTOR = APP_ADDRESS;

    /*
     * Set application's stack pointer
     * and branch directly to Reset_Handler.
     */
    __asm volatile (
        "msr msp, %0\n"
        "bx  %1\n"
        :
        : "r" (app_msp), "r" (app_reset)
        : "memory"
    );

    /* Should never reach here */
    while (1)
    {
    }
}


/*----------------------------------------------------------
 * Bootloader main
 *----------------------------------------------------------*/
int main(void)
{
    /* Enable GPIOA clock */
    RCC_AHB1ENR |= (1U << 0);

    /* Configure PA5 as output */
    GPIOA_MODER &= ~(3U << (5 * 2));
    GPIOA_MODER |=  (1U << (5 * 2));

    /*
     * Turn LED ON briefly.
     * This shows that the bootloader executed.
     */
    GPIOA_ODR |= (1U << 5);

    for (volatile uint32_t i = 0; i < 300000; i++)
    {
    }

    /*
     * Check whether a valid application exists.
     */
    if (is_application_valid())
    {
        jump_to_application();
    }

    /*
     * No valid application.
     * Stay in bootloader.
     */
    while (1)
    {
    }
}