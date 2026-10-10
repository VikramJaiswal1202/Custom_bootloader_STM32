#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart2_init(void);
void uart2_write_char(uint8_t data);
uint8_t uart2_read_char(void);

#endif
