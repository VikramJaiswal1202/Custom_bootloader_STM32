#ifndef APP_H
#define APP_H

#include <stdint.h>

int app_is_valid(uint32_t app_address);

void app_jump(uint32_t app_address);

#endif
