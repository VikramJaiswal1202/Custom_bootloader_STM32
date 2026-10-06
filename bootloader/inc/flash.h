#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>

void flash_unlock(void);
void flash_lock(void);

void flash_wait_ready(void);
int flash_has_error(void);

int flash_erase_sector(uint32_t sector);

int flash_program_word(uint32_t address, uint32_t data);

#endif