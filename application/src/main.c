
#include <stdint.h>

/* RCC registers */
#define RCC_AHB1ENR     (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB1ENR     (*(volatile uint32_t *)0x40023840UL)

/* GPIOA registers */
#define GPIOA_MODER     (*(volatile uint32_t *)0x40020000UL)
#define GPIOA_ODR       (*(volatile uint32_t *)0x40020014UL)
#define GPIOA_AFRL      (*(volatile uint32_t *)0x40020020UL)

/* USART2 registers */
#define USART2_SR       (*(volatile uint32_t *)0x40004400UL)
#define USART2_DR       (*(volatile uint32_t *)0x40004404UL)
#define USART2_BRR      (*(volatile uint32_t *)0x40004408UL)
#define USART2_CR1      (*(volatile uint32_t *)0x4000440CUL)
#define USART2_CR2      (*(volatile uint32_t *)0x40004410UL)
#define USART2_CR3      (*(volatile uint32_t *)0x40004414UL)

/* USART status bits */
#define USART_SR_RXNE   (1U << 5)
#define USART_SR_TXE    (1U << 7)

/* USART control bits */
#define USART_CR1_RE    (1U << 2)
#define USART_CR1_TE    (1U << 3)
#define USART_CR1_UE    (1U << 13)

#define LED_PIN         5U
#define CMD_BUFFER_SIZE 32U

#define FIRMWARE_VERSION "1.2.0"

static char command_buffer[CMD_BUFFER_SIZE];
static uint32_t command_index = 0U;

/* Configure USART2 on PA2 (TX) and PA3 (RX). */
static void uart2_init(void)
{
    /* Enable GPIOA and USART2 clocks. */
    RCC_AHB1ENR |= (1U << 0);
    RCC_APB1ENR |= (1U << 17);

    /* PA2 and PA3: alternate-function mode. */
    GPIOA_MODER &= ~((3U << 4) | (3U << 6));
    GPIOA_MODER |=  ((2U << 4) | (2U << 6));

    /* AF7 selects USART2 for PA2 and PA3. */
    GPIOA_AFRL &= ~((0xFU << 8) | (0xFU << 12));
    GPIOA_AFRL |=  ((7U << 8) | (7U << 12));

    /* Configure USART2. */
    USART2_CR1 = 0U;
    USART2_CR2 = 0U;
    USART2_CR3 = 0U;

    /*
     * Assumes USART2 peripheral clock is 16 MHz.
     * 16,000,000 / 115,200 = 138.89, rounded to 139.
     */
    USART2_BRR = 139U;

    /* Enable transmitter, receiver and USART. */
    USART2_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void uart2_write_char(uint8_t data)
{
    while ((USART2_SR & USART_SR_TXE) == 0U)
    {
    }

    USART2_DR = data;
}

static void uart2_write_string(const char *str)
{
    while (*str != '\0')
    {
        uart2_write_char((uint8_t)*str);
        str++;
    }
}

static int strings_equal(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0')
    {
        if (*a != *b)
        {
            return 0;
        }

        a++;
        b++;
    }

    return (*a == '\0' && *b == '\0');
}

/* Handle a complete command. */
static void process_command(const char *command)
{
    if (strings_equal(command, "GET_VERSION"))
    {
        uart2_write_string("Firmware version: ");
        uart2_write_string(FIRMWARE_VERSION);
        uart2_write_string("\r\n");
    }
    else
    {
        uart2_write_string("ERROR: UNKNOWN_COMMAND\r\n");
    }
}

/*
 * Poll UART without blocking the LED blink loop.
 * Commands end with CR or LF.
 */
static void uart2_poll_command(void)
{
    if ((USART2_SR & USART_SR_RXNE) == 0U)
    {
        return;
    }

    char ch = (char)(USART2_DR & 0xFFU);

    if (ch == '\r' || ch == '\n')
    {
        if (command_index > 0U)
        {
            command_buffer[command_index] = '\0';
            process_command(command_buffer);
            command_index = 0U;
        }
    }
    else if (command_index < CMD_BUFFER_SIZE - 1U)
    {
        command_buffer[command_index++] = ch;
    }
    else
    {
        command_index = 0U;
        uart2_write_string("ERROR: COMMAND_TOO_LONG\r\n");
    }
}

int main(void)
{
    /* Initialize USART2. */
    uart2_init();

    /* Configure PA5 as a general-purpose output for LD2. */
    RCC_AHB1ENR |= (1U << 0);

    GPIOA_MODER &= ~(3U << (LED_PIN * 2U));
    GPIOA_MODER |=  (1U << (LED_PIN * 2U));

    /* Report application startup and its firmware version. */
    uart2_write_string("\r\nApplication started\r\n");
    uart2_write_string("Firmware version: ");
    uart2_write_string(FIRMWARE_VERSION);
    uart2_write_string("\r\n");
    uart2_write_string("Send GET_VERSION to query the version\r\n");

    while (1)
    {
        GPIOA_ODR ^= (1U << LED_PIN);

        /* Keep checking UART during the delay. */
        for (volatile uint32_t i = 0U; i < 100000U; i++)
        {
            uart2_poll_command();
        }
    }
}
