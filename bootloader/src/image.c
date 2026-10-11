
#include <stdint.h>

#include "image.h"
#include "flash.h"
#include "crc.h"

#define APP_FLASH_START   0x08008000UL
#define APP_FLASH_END     0x08080000UL

int image_erase(uint32_t address, uint32_t size)
{
    uint32_t end;
    uint32_t sector;

    if (address < APP_FLASH_START ||
        address >= APP_FLASH_END ||
        size == 0U)
    {
        return -1;
    }

    end = address + size;

    if (end < address || end > APP_FLASH_END)
    {
        return -1;
    }

    /* Find the first Flash sector touched by the image. */
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

    while (sector <= 7U)
    {
        uint32_t sector_start;

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

        if (sector_start >= end)
        {
            break;
        }

        if (flash_erase_sector(sector) != 0)
        {
            return -1;
        }

        sector++;
    }

    return 0;
}

int image_program(uint32_t address,
                  const uint8_t *data,
                  uint32_t size)
{
    uint32_t i = 0U;

    if (address < APP_FLASH_START ||
        address >= APP_FLASH_END ||
        size == 0U ||
        size > (APP_FLASH_END - address))
    {
        return -1;
    }

    /* Program complete 32-bit words. */
    for (; i + 4U <= size; i += 4U)
    {
        uint32_t word =
            ((uint32_t)data[i]) |
            ((uint32_t)data[i + 1U] << 8U) |
            ((uint32_t)data[i + 2U] << 16U) |
            ((uint32_t)data[i + 3U] << 24U);

        if (flash_program_word(address + i, word) != 0)
        {
            return -1;
        }
    }

    /* Program any remaining bytes, padded with erased 0xFF bytes. */
    if (i < size)
    {
        uint32_t word = 0xFFFFFFFFUL;
        uint32_t remaining = size - i;

        for (uint32_t j = 0U; j < remaining; j++)
        {
            uint32_t shift = 8U * j;
            word &= ~(0xFFUL << shift);
            word |= ((uint32_t)data[i + j] << shift);
        }

        if (flash_program_word(address + i, word) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int image_verify(uint32_t address,
                 const uint8_t *data,
                 uint32_t size)
{
    volatile const uint8_t *flash_data;

    if (address < APP_FLASH_START ||
        address >= APP_FLASH_END ||
        size == 0U ||
        size > (APP_FLASH_END - address))
    {
        return -1;
    }

    flash_data = (volatile const uint8_t *)address;

    for (uint32_t i = 0U; i < size; i++)
    {
        if (flash_data[i] != data[i])
        {
            return -1;
        }
    }

    return 0;
}

uint32_t image_crc32(uint32_t address, uint32_t size)
{
    volatile const uint8_t *flash_data;

    if (address < APP_FLASH_START ||
        address >= APP_FLASH_END ||
        size == 0U ||
        size > (APP_FLASH_END - address))
    {
        return 0U;
    }

    flash_data = (volatile const uint8_t *)address;

    return crc32_calculate((const uint8_t *)flash_data, size);
}