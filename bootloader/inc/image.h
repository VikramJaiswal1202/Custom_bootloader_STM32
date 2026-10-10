#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>

int image_erase(uint32_t address, uint32_t size);

int image_program(uint32_t address,
const uint8_t *data,
uint32_t size);

int image_verify(uint32_t address,
const uint8_t *data,
uint32_t size);

#endif
