#include "app.h"

/* Application Flash boundaries */
#define APP_FLASH_START  0x08008000UL
#define APP_FLASH_END    0x08080000UL

/* SRAM boundaries */
#define SRAM_START       0x20000000UL
#define SRAM_END         0x20020000UL

int app_is_valid(uint32_t app_address)
{
uint32_t initial_msp;
uint32_t reset_handler;


/* Application must start inside application Flash. */
if (app_address < APP_FLASH_START ||
    app_address > (APP_FLASH_END - 8U))
{
    return 0;
}

/* Read the first two vector-table entries. */
initial_msp = *(volatile const uint32_t *)app_address;
reset_handler = *(volatile const uint32_t *)(app_address + 4U);

/* Check that the initial stack pointer is in SRAM. */
if (initial_msp < SRAM_START ||
    initial_msp > SRAM_END ||
    (initial_msp & 0x3U) != 0U)
{
    return 0;
}

/* Cortex-M handlers must have the Thumb bit set. */
if ((reset_handler & 1U) == 0U)
{
    return 0;
}

/* Check the reset handler address, ignoring the Thumb bit. */
reset_handler &= ~1U;

if (reset_handler < APP_FLASH_START ||
    reset_handler >= APP_FLASH_END)
{
    return 0;
}

return 1;

}
/* Cortex-M4 System Control Block registers */
#define SCB_VTOR (*(volatile uint32_t *)0xE000ED08UL)

/* Disable interrupts and transfer control to the application */
void app_jump(uint32_t app_address)
{
uint32_t app_msp;
uint32_t app_reset_handler;


/* Never jump to an invalid application. */
if (!app_is_valid(app_address))
{
    return;
}

/* Read the application's vector table. */
app_msp = *(volatile const uint32_t *)app_address;

app_reset_handler =
    *(volatile const uint32_t *)(app_address + 4U);

/*
 * Stop interrupts before changing execution context.
 */
__asm volatile ("cpsid i" ::: "memory");

/*
 * Point the vector table to the application.
 */
SCB_VTOR = app_address;

/*
 * Ensure the vector-table update is visible.
 */
__asm volatile ("dsb" ::: "memory");
__asm volatile ("isb" ::: "memory");

/*
 * Set MSP and branch to the application's reset handler.
 *
 * Do not return to the bootloader afterward.
 */
__asm volatile (
    "msr msp, %0    \n"
    "bx %1          \n"
    :
    : "r" (app_msp), "r" (app_reset_handler)
    : "memory"
);

/* Execution should never reach here. */
while (1)
{
}


}
