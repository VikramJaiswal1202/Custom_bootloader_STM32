#include "uart.h"

#define RCC_AHB1ENR   (*(volatile unsigned int *)0x40023830UL)
#define RCC_APB1ENR   (*(volatile unsigned int *)0x40023840UL)

#define GPIOA_MODER   (*(volatile unsigned int *)0x40020000UL)
#define GPIOA_AFRL    (*(volatile unsigned int *)0x40020020UL)

#define USART2_SR     (*(volatile unsigned int *)0x40004400UL)
#define USART2_DR     (*(volatile unsigned int *)0x40004404UL)
#define USART2_BRR    (*(volatile unsigned int *)0x40004408UL)
#define USART2_CR1    (*(volatile unsigned int *)0x4000440CUL)
#define USART2_CR2    (*(volatile unsigned int *)0x40004410UL)
#define USART2_CR3    (*(volatile unsigned int *)0x40004414UL)

#define USART_SR_RXNE (1U << 5)
#define USART_SR_TXE  (1U << 7)

#define USART_CR1_RE  (1U << 2)
#define USART_CR1_TE  (1U << 3)
#define USART_CR1_UE  (1U << 13)

void uart2_init(void)
{
/* Enable GPIOA and USART2 clocks. */
RCC_AHB1ENR |= (1U << 0);
RCC_APB1ENR |= (1U << 17);


/* PA2 and PA3: alternate function mode. */
GPIOA_MODER &= ~((3U << 4) | (3U << 6));
GPIOA_MODER |=  ((2U << 4) | (2U << 6));

/* Select AF7 (USART2) for PA2 and PA3. */
GPIOA_AFRL &= ~((0xFU << 8) | (0xFU << 12));
GPIOA_AFRL |=  ((7U << 8) | (7U << 12));

/* Disable USART before configuring it. */
USART2_CR1 = 0U;
USART2_CR2 = 0U;
USART2_CR3 = 0U;

/* 16 MHz / 115200 baud = 138.89; rounded to 139. */
USART2_BRR = 139U;

/* Enable transmitter, receiver, and USART. */
USART2_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;


}

void uart2_write_char(uint8_t data)
{
while ((USART2_SR & USART_SR_TXE) == 0U)
{
}

USART2_DR = data;


}

uint8_t uart2_read_char(void)
{
while ((USART2_SR & USART_SR_RXNE) == 0U)
{
}

return (uint8_t)(USART2_DR & 0xFFU);
    

}
