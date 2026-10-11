
#include "crc.h"

uint32_t crc32_calculate(const uint8_t *data, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFFU;

    for (uint32_t i = 0U; i < size; i++)
    {
        crc ^= data[i];

        for (uint32_t bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;
}