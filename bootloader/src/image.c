#include "image.h"
#include "flash.h"


/* =========================
 * Application Flash limits
 * ========================= */

#define APP_FLASH_START   0x08008000UL
#define APP_FLASH_END     0x08080000UL


/* =========================
 * Sector erase
 * ========================= */

int image_erase(uint32_t address, uint32_t size)
{
    uint32_t end;
    uint32_t sector;

    /*
     * Check starting address.
     */
    if (address < APP_FLASH_START ||
        address >= APP_FLASH_END)
    {
        return -1;
    }

    /*
     * Image must not be empty.
     */
    if (size == 0U)
    {
        return -1;
    }

    /*
     * Calculate the end address.
     */
    end = address + size;

    /*
     * Check for integer overflow
     * and application Flash boundary.
     */
    if (end < address || end > APP_FLASH_END)
    {
        return -1;
    }


    /* =========================
     * Determine first sector
     * ========================= */

    if (address < 0x0800C000UL)
    {
        sector = 2U;
    }
    else if (address < 0x08010000UL)
    {
        sector = 3U;
    }
    else if (address < 0x08020000UL)
    {
        sector = 4U;
    }
    else if (address < 0x08040000UL)
    {
        sector = 5U;
    }
    else if (address < 0x08060000UL)
    {
        sector = 6U;
    }
    else
    {
        sector = 7U;
    }


    /* =========================
     * Erase all sectors touched
     * by the image
     * ========================= */

    while (sector <= 7U)
    {
        uint32_t sector_start;


        /*
         * Get the starting address
         * of the current sector.
         */
        if (sector == 2U)
        {
            sector_start = 0x08008000UL;
        }
        else if (sector == 3U)
        {
            sector_start = 0x0800C000UL;
        }
        else if (sector == 4U)
        {
            sector_start = 0x08010000UL;
        }
        else if (sector == 5U)
        {
            sector_start = 0x08020000UL;
        }
        else if (sector == 6U)
        {
            sector_start = 0x08040000UL;
        }
        else
        {
            sector_start = 0x08060000UL;
        }


        /*
         * If the current sector starts
         * at or after the image end,
         * the image does not touch it.
         */
        if (sector_start >= end)
        {
            break;
        }


        /*
         * Erase the current sector.
         */
        if (flash_erase_sector(sector) != 0)
        {
            return -1;
        }


        sector++;
    }

    return 0;
}


/* =========================
 * Program firmware image
 * ========================= */

int image_program(uint32_t address,
                  const uint8_t *data,
                  uint32_t size)
{
    uint32_t i;
    uint32_t word;
    uint32_t remaining;


    /*
     * Check that the starting address
     * is inside application Flash.
     */
    if (address < APP_FLASH_START ||
        address >= APP_FLASH_END)
    {
        return -1;
    }


    /*
     * Check that the image fits
     * completely inside application Flash.
     */
    if (size > (APP_FLASH_END - address))
    {
        return -1;
    }


    /*
     * Program complete 32-bit words.
     *
     * STM32F446 uses little-endian memory,
     * so the first byte occupies the
     * least significant byte.
     */
    for (i = 0U; i + 4U <= size; i += 4U)
    {
        word = ((uint32_t)data[i]) |
               ((uint32_t)data[i + 1U] << 8) |
               ((uint32_t)data[i + 2U] << 16) |
               ((uint32_t)data[i + 3U] << 24);


        /*
         * Program one 32-bit word.
         */
        if (flash_program_word(address + i, word) != 0)
        {
            return -1;
        }
    }


    /*
     * Handle remaining 1-3 bytes.
     */
    remaining = size - i;

    if (remaining != 0U)
    {
        /*
         * Start with an erased Flash word.
         *
         * Erased Flash = 0xFFFFFFFF.
         */
        word = 0xFFFFFFFFUL;


        /*
         * Insert the remaining bytes.
         */
        for (uint32_t j = 0U; j < remaining; j++)
        {
            word &= ~(0xFFUL << (8U * j));

            word |= ((uint32_t)data[i + j])
                    << (8U * j);
        }


        /*
         * Program the final word.
         */
        if (flash_program_word(address + i, word) != 0)
        {
            return -1;
        }
    }


    return 0;
}