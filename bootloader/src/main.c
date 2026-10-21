
#include <stdint.h>
#include "boot.h"
#include "uart.h"
#include "flash.h"
#include "image.h"

#define USART2_STATUS_REG (*(volatile uint32_t *)0x40004400UL)
#define USART_SR_TC       (1U << 6)

#define APP_START_ADDRESS    0x08008000UL
#define FLASH_END_ADDRESS    0x08080000UL
#define MAX_APP_SIZE         (FLASH_END_ADDRESS - APP_START_ADDRESS)

#define CMD_BUFFER_SIZE      32U
#define TRANSFER_CHUNK_SIZE  128U
static uint8_t application_verified = 0U;

static void uart_write_string(const char *str)
{
    while (*str != '\0')
    {
        uart2_write_char((uint8_t)*str++);
    }
}

static void debug_print_command(const char *command)
{
    uart_write_string("CMD=[");

    while (*command != '\0')
    {
        uart2_write_char((uint8_t)*command++);
    }

    uart_write_string("]\r\n");
}

static void uart_write_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";

    for (int i = 7; i >= 0; i--)
    {
        uint8_t digit = (uint8_t)((value >> (i * 4)) & 0x0FU);
        uart2_write_char((uint8_t)hex[digit]);
    }

    uart_write_string("\r\n");
}

static int verify_chunk(uint32_t address,
                        const uint8_t *data,
                        uint32_t size)
{
    volatile const uint8_t *flash_data;

    if (address < APP_START_ADDRESS ||
        address >= FLASH_END_ADDRESS ||
        size == 0U ||
        size > (FLASH_END_ADDRESS - address))
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

/* Parse a decimal image size. Returns zero if invalid. */
static uint32_t parse_size(const char *str)
{
    uint32_t value = 0U;

    if (*str == '\0')
    {
        return 0U;
    }

    while (*str != '\0')
    {
        uint32_t digit;

        if (*str < '0' || *str > '9')
        {
            return 0U;
        }

        digit = (uint32_t)(*str - '0');

        /* Prevent overflow and reject sizes beyond Flash capacity. */
        if (value > (MAX_APP_SIZE - digit) / 10U)
        {
            return 0U;
        }

        value = (value * 10U) + digit;
        str++;
    }

    return value;
}

/*
 * Parse exactly eight hexadecimal characters.
 * Returns zero on success and -1 on failure.
 */
static int parse_crc32(const char *str, uint32_t *result)
{
    uint32_t value = 0U;

    for (uint32_t i = 0U; i < 8U; i++)
    {
        char c = str[i];
        uint32_t digit;

        if (c >= '0' && c <= '9')
        {
            digit = (uint32_t)(c - '0');
        }
        else if (c >= 'A' && c <= 'F')
        {
            digit = (uint32_t)(c - 'A') + 10U;
        }
        else if (c >= 'a' && c <= 'f')
        {
            digit = (uint32_t)(c - 'a') + 10U;
        }
        else
        {
            return -1;
        }

        value = (value << 4U) | digit;
    }

    if (str[8] != '\0')
    {
        return -1;
    }

    *result = value;
    return 0;
}

static void receive_firmware(uint32_t size, uint32_t expected_crc)
{
    application_verified = 0U;
    uint8_t buffer[TRANSFER_CHUNK_SIZE];
    uint32_t received = 0U;

    uart_write_string("PREPARING\r\n");

    flash_unlock();

    if (image_erase(APP_START_ADDRESS, size) != 0)
    {
        flash_lock();
        uart_write_string("ERROR: FLASH_ERASE_FAILED\r\n");
        return;
    }

    uart_write_string("READY\r\n");

    while (received < size)
    {
        uint32_t chunk_size = size - received;

        if (chunk_size > TRANSFER_CHUNK_SIZE)
        {
            chunk_size = TRANSFER_CHUNK_SIZE;
        }

        /* Receive one complete chunk. */
        for (uint32_t i = 0U; i < chunk_size; i++)
        {
            buffer[i] = uart2_read_char();
        }

        /* Program this chunk into application Flash. */
        if (image_program(APP_START_ADDRESS + received,
                          buffer,
                          chunk_size) != 0)
        {
            flash_lock();
            uart_write_string("ERROR: FLASH_PROGRAM_FAILED\r\n");
            return;
        }

        /* Read the programmed bytes back and compare. */
        if (verify_chunk(APP_START_ADDRESS + received,
                         buffer,
                         chunk_size) != 0)
        {
            flash_lock();
            uart_write_string("ERROR: CHUNK_VERIFY_FAILED\r\n");
            return;
        }

        received += chunk_size;
        uart_write_string("ACK\r\n");
    }

    flash_lock();

    /* Verify the complete image using CRC-32. */
    uint32_t actual_crc = image_crc32(APP_START_ADDRESS, size);

    uart_write_string("EXPECTED_CRC: ");
    uart_write_hex32(expected_crc);

    uart_write_string("ACTUAL_CRC: ");
    uart_write_hex32(actual_crc);

if (actual_crc != expected_crc)
{
    uart_write_string("ERROR: CRC_MISMATCH\r\n");
    return;
}

if (boot_validate_application() != 0)
{
    uart_write_string("ERROR: INVALID_APPLICATION\r\n");
    return;
}

application_verified = 1U;

/* Tell the host that the firmware update succeeded. */
uart_write_string("UPDATE_OK\r\n");

/* Wait until the final response has physically transmitted. */
while ((USART2_STATUS_REG & USART_SR_TC) == 0U)
{
}
uart_write_string("AUTO_BOOT: JUMPING\r\n");

/* Wait for the diagnostic message to finish transmitting. */
while ((USART2_STATUS_REG & USART_SR_TC) == 0U)
{
}

boot_jump_to_application();

uart_write_string("ERROR: BOOT_RETURNED\r\n");
}

static void process_command(char *command)
{
    static const char info[] = "GET_INFO";
    static const char prefix[] = "BEGIN_UPDATE ";

    uint32_t i = 0U;

    /* Handle GET_INFO. */
    while (info[i] != '\0' && command[i] == info[i])
    {
        i++;
    }

    if (info[i] == '\0' && command[i] == '\0')
    {
        uart_write_string("BOOTLOADER_OK\r\n");
        return;
    }
    if (command[0] == 'B' &&
    command[1] == 'O' &&
    command[2] == 'O' &&
    command[3] == 'T' &&
    command[4] == '\0')
{
    if (application_verified == 0U)
    {
        uart_write_string("ERROR: APPLICATION_NOT_VERIFIED\r\n");
        return;
    }

    uart_write_string("BOOTING_APPLICATION\r\n");
    boot_jump_to_application();

    /* Reached only if the jump routine returns. */
    uart_write_string("ERROR: BOOT_RETURNED\r\n");
    return;
}

    /* Check the complete BEGIN_UPDATE prefix. */
    i = 0U;

    while (prefix[i] != '\0' && command[i] == prefix[i])
    {
        i++;
    }

    if (prefix[i] != '\0')
    {
        uart_write_string("ERROR: UNKNOWN_COMMAND\r\n");
        return;
    }

    /*
     * command[i] now points to the image size.
     * Expected format: BEGIN_UPDATE <size> <8-digit CRC>
     */
    char *size_str = &command[i];
    char *separator = size_str;

    while (*separator != '\0' && *separator != ' ')
    {
        separator++;
    }

    if (*separator != ' ')
    {
        uart_write_string("ERROR: INVALID_COMMAND\r\n");
        return;
    }

    *separator = '\0';
    char *crc_str = separator + 1;

    /* Reject an empty CRC or extra spaces. */
    if (*crc_str == '\0' || *crc_str == ' ')
    {
        uart_write_string("ERROR: INVALID_CRC\r\n");
        return;
    }

    uint32_t size = parse_size(size_str);
    uint32_t expected_crc;

    if (size == 0U || size > MAX_APP_SIZE)
    {
        uart_write_string("ERROR: INVALID_SIZE\r\n");
        return;
    }

    if (parse_crc32(crc_str, &expected_crc) != 0)
    {
        uart_write_string("ERROR: INVALID_CRC\r\n");
        return;
    }

    receive_firmware(size, expected_crc);
}

int main(void)
{
    
    char command[CMD_BUFFER_SIZE];
    uint32_t index = 0U;

    uart2_init();
    uart_write_string("BOOTLOADER_OK\r\n");

    while (1)
    {
        uint8_t ch = uart2_read_char();

        if (ch == '\r' || ch == '\n')
        {
            if (index > 0U)
            {
                command[index] = '\0';
                debug_print_command(command);
                process_command(command);
                index = 0U;
            }
        }
        else if (index < CMD_BUFFER_SIZE - 1U)
        {
            command[index++] = (char)ch;
        }
        else
        {
            index = 0U;
            uart_write_string("ERROR: COMMAND_TOO_LONG\r\n");
        }
    }
}