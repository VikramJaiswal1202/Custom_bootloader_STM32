
#include <stdint.h>

#include "uart.h"
#include "flash.h"
#include "image.h"

/* Application Flash layout */
#define APP_START_ADDRESS   0x08008000UL
#define FLASH_END_ADDRESS  0x08080000UL
#define MAX_APP_SIZE       (FLASH_END_ADDRESS - APP_START_ADDRESS)

/* UART protocol */
#define CMD_BUFFER_SIZE     32U
#define TRANSFER_CHUNK_SIZE 128U

static void uart_write_string(const char *str)
{
    while (*str != '\0')
    {
        uart2_write_char((uint8_t)*str);
        str++;
    }
}

/*
 * Compare a programmed Flash chunk with the received bytes.
 * Returns 0 on success, -1 on mismatch or invalid range.
 */
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

/*
 * Receive firmware over UART and program it into Flash.
 *
 * Protocol:
 *   PREPARING
 *   READY
 *   [128-byte data chunk] -> ACK
 *   ...
 *   UPDATE_OK
 */
static void receive_firmware(uint32_t size)
{
    uint8_t buffer[TRANSFER_CHUNK_SIZE];
    uint32_t received = 0U;

    uart_write_string("PREPARING\r\n");

    /*
     * Erase the Flash sectors occupied by the new image.
     */
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

        /*
         * Receive one chunk.
         */
        for (uint32_t i = 0U; i < chunk_size; i++)
        {
            buffer[i] = uart2_read_char();
        }

        /*
         * Program the chunk into application Flash.
         */
        if (image_program(APP_START_ADDRESS + received,
                          buffer,
                          chunk_size) != 0)
        {
            flash_lock();
            uart_write_string("ERROR: FLASH_PROGRAM_FAILED\r\n");
            return;
        }

        /*
         * Read the programmed bytes back and compare them.
         */
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

    /*
     * All chunks were programmed and read-back verified.
     */
    flash_lock();

    uart_write_string("UPDATE_OK\r\n");
}

/*
 * Parse a positive decimal integer.
 * Returns 0 for invalid input.
 */
static uint32_t parse_size(const char *str)
{
    uint32_t value = 0U;

    if (*str == '\0')
    {
        return 0U;
    }

    while (*str != '\0')
    {
        if (*str < '0' || *str > '9')
        {
            return 0U;
        }

        uint32_t digit = (uint32_t)(*str - '0');

        if (value > (MAX_APP_SIZE - digit) / 10U)
        {
            return 0U;
        }

        value = (value * 10U) + digit;
        str++;
    }

    return value;
}

static void process_command(char *command)
{
    static const char begin_prefix[] = "BEGIN_UPDATE ";

    /*
     * GET_INFO command.
     */
    if (command[0] == 'G' &&
        command[1] == 'E' &&
        command[2] == 'T' &&
        command[3] == '_' &&
        command[4] == 'I' &&
        command[5] == 'N' &&
        command[6] == 'F' &&
        command[7] == 'O' &&
        command[8] == '\0')
    {
        uart_write_string("BOOTLOADER_OK\r\n");
        return;
    }

    /*
     * BEGIN_UPDATE <size> command.
     */
    uint32_t i = 0U;

    while (begin_prefix[i] != '\0' &&
           command[i] == begin_prefix[i])
    {
        i++;
    }

    if (begin_prefix[i] == '\0')
    {
        uint32_t size = parse_size(&command[i]);

        if (size == 0U || size > MAX_APP_SIZE)
        {
            uart_write_string("ERROR: INVALID_SIZE\r\n");
            return;
        }

        receive_firmware(size);
        return;
    }

    uart_write_string("ERROR: UNKNOWN_COMMAND\r\n");
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
                process_command(command);
                index = 0U;
            }
        }
        else
        {
            if (index < (CMD_BUFFER_SIZE - 1U))
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
}