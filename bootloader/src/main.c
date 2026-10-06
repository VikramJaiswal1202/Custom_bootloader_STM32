#include <stdint.h>
#include "image.h"

#define APP_ADDRESS 0x08008000UL

static const uint8_t test_image[] =
{
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB
};

int main(void)
{
    /*
     * Erase every sector required
     * by the image.
     */
    image_erase(APP_ADDRESS, sizeof(test_image));

    /*
     * Program the image.
     */
    image_program(APP_ADDRESS,
                  test_image,
                  sizeof(test_image));

    while (1)
    {
    }
}