#include "flash.h"

/* =========================
 * FLASH registers
 * ========================= */

#define FLASH_KEYR   (*(volatile uint32_t *)0x40023C04UL)
#define FLASH_SR     (*(volatile uint32_t *)0x40023C0CUL)
#define FLASH_CR     (*(volatile uint32_t *)0x40023C10UL)


/* =========================
 * FLASH unlock keys
 * ========================= */

#define FLASH_KEY1   0x45670123UL
#define FLASH_KEY2   0xCDEF89ABUL


/* =========================
 * FLASH_SR bits
 * ========================= */

#define FLASH_SR_EOP      (1U << 0)
#define FLASH_SR_OPERR    (1U << 1)
#define FLASH_SR_WRPERR   (1U << 4)
#define FLASH_SR_PGAERR   (1U << 5)
#define FLASH_SR_PGPERR   (1U << 6)
#define FLASH_SR_PGSERR   (1U << 7)
#define FLASH_SR_RDERR    (1U << 8)
#define FLASH_SR_BSY      (1U << 16)

#define FLASH_SR_ERRORS   (FLASH_SR_OPERR  | \
                          FLASH_SR_WRPERR | \
                          FLASH_SR_PGAERR | \
                          FLASH_SR_PGPERR | \
                          FLASH_SR_PGSERR | \
                          FLASH_SR_RDERR)


/* =========================
 * FLASH_CR bits
 * ========================= */

#define FLASH_CR_PG       (1U << 0)
#define FLASH_CR_SER      (1U << 1)

#define FLASH_CR_SNB_Pos  3U
#define FLASH_CR_SNB_Msk  (0xFU << FLASH_CR_SNB_Pos)

#define FLASH_CR_PSIZE_Pos 8U
#define FLASH_CR_PSIZE_Msk (3U << FLASH_CR_PSIZE_Pos)

#define FLASH_CR_STRT     (1U << 16)
#define FLASH_CR_LOCK     (1U << 31)


/* =========================
 * Application Flash limits
 * ========================= */

#define APP_FLASH_START   0x08008000UL
#define APP_FLASH_END     0x08080000UL


/* =========================
 * Flash unlock
 * ========================= */

void flash_unlock(void)
{
    if (FLASH_CR & FLASH_CR_LOCK)
    {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
}


/* =========================
 * Flash lock
 * ========================= */

void flash_lock(void)
{
    FLASH_CR |= FLASH_CR_LOCK;
}


/* =========================
 * Wait until Flash is ready
 * ========================= */

void flash_wait_ready(void)
{
    while (FLASH_SR & FLASH_SR_BSY)
    {
    }
}


/* =========================
 * Check Flash errors
 * ========================= */

int flash_has_error(void)
{
    return (FLASH_SR & FLASH_SR_ERRORS) != 0U;
}


/* =========================
 * Erase one application sector
 * ========================= */

int flash_erase_sector(uint32_t sector)
{
    /*
     * Sectors 0 and 1 contain
     * the bootloader.
     *
     * Only allow sectors 2-7.
     */
    if (sector < 2U || sector > 7U)
    {
        return -1;
    }

    /*
     * Unlock Flash.
     */
    flash_unlock();

    /*
     * Wait until Flash is ready.
     */
    flash_wait_ready();

    /*
     * Clear previous status flags.
     *
     * Writing 1 clears these flags.
     */
    FLASH_SR = FLASH_SR_EOP | FLASH_SR_ERRORS;

    /*
     * Clear previous sector number.
     */
    FLASH_CR &= ~FLASH_CR_SNB_Msk;

    /*
     * Select requested sector.
     *
     * SNB occupies bits [6:3].
     */
    FLASH_CR |= (sector << FLASH_CR_SNB_Pos);

    /*
     * Select sector erase mode.
     */
    FLASH_CR |= FLASH_CR_SER;

    /*
     * Start erase.
     */
    FLASH_CR |= FLASH_CR_STRT;

    /*
     * Wait for erase to complete.
     */
    flash_wait_ready();

    /*
     * Disable sector erase mode.
     */
    FLASH_CR &= ~FLASH_CR_SER;

    /*
     * Check for errors.
     */
    if (flash_has_error())
    {
        flash_lock();
        return -1;
    }

    /*
     * Lock Flash again.
     */
    flash_lock();

    return 0;
}


/* =========================
 * Program one 32-bit word
 * ========================= */

int flash_program_word(uint32_t address, uint32_t data)
{
    /*
     * Address must be 4-byte aligned.
     */
    if ((address & 0x3U) != 0U)
    {
        return -1;
    }

    /*
     * Only allow writing to
     * application Flash.
     */
    if (address < APP_FLASH_START ||
        address >= APP_FLASH_END)
    {
        return -1;
    }

    /*
     * Unlock Flash.
     */
    flash_unlock();

    /*
     * Wait until Flash is ready.
     */
    flash_wait_ready();

    /*
     * Clear previous status flags.
     */
    FLASH_SR = FLASH_SR_EOP | FLASH_SR_ERRORS;

    /*
     * Select 32-bit programming.
     *
     * PSIZE = 10
     */
    FLASH_CR &= ~FLASH_CR_PSIZE_Msk;
    FLASH_CR |= (2U << FLASH_CR_PSIZE_Pos);

    /*
     * Enable programming.
     */
    FLASH_CR |= FLASH_CR_PG;

    /*
     * Write the 32-bit data.
     */
    *(volatile uint32_t *)address = data;

    /*
     * Wait until programming finishes.
     */
    flash_wait_ready();

    /*
     * Disable programming mode.
     */
    FLASH_CR &= ~FLASH_CR_PG;

    /*
     * Check for programming errors.
     */
    if (flash_has_error())
    {
        flash_lock();
        return -1;
    }

    /*
     * Verify the programmed value.
     */
    if (*(volatile uint32_t *)address != data)
    {
        flash_lock();
        return -1;
    }

    /*
     * Lock Flash.
     */
    flash_lock();

    return 0;
}