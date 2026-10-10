
#include "uart.h"
#include <stdint.h>
#include <limits.h>

#define CMD_BUFFER_SIZE 32U



#define APP_START_ADDRESS  0x08008000UL
#define FLASH_END_ADDRESS  0x08080000UL
#define MAX_APP_SIZE       (FLASH_END_ADDRESS - APP_START_ADDRESS)

static uint32_t parse_size(const char *str, uint32_t *size)
{
    uint32_t value = 0U;

    if (*str == '\0')
        return 0U;

    while (*str != '\0')
    {
        if ((*str < '0') || (*str > '9'))
            return 0U;

        uint32_t digit = (uint32_t)(*str - '0');

        if (value > (UINT32_MAX - digit) / 10U)
            return 0U;

        value = value * 10U + digit;
        str++;
    }

    *size = value;
    return 1U;
}


static void uart_write_string(const char *str)
{
    while (*str != '\0')
    {
        uart2_write_char((uint8_t)*str);
        str++;
    }
}


static void process_command(const char *cmd)
{
    const char begin[] = "BEGIN_UPDATE ";
    uint32_t i = 0U;

    while ((begin[i] != '\0') && (cmd[i] == begin[i]))
        i++;

    if (begin[i] == '\0')
    {
        uint32_t size = 0U;

        if (!parse_size(&cmd[i], &size) ||
            (size == 0U) ||
            (size > MAX_APP_SIZE))
        {
            uart_write_string("ERROR: INVALID_SIZE\r\n");
            return;
        }

        uart_write_string("READY\r\n");
        return;
    }

    const char info[] = "GET_INFO";
    i = 0U;

    while ((info[i] != '\0') && (cmd[i] == info[i]))
        i++;

    if ((info[i] == '\0') && (cmd[i] == '\0'))
    {
        uart_write_string("BOOTLOADER_OK\r\n");
    }
    else
    {
        uart_write_string("ERROR: UNKNOWN_COMMAND\r\n");
    }
}
int main(void)
{
    char cmd[CMD_BUFFER_SIZE];
    uint32_t index = 0U;

    uart2_init();
    uart_write_string("\r\nBOOTLOADER READY\r\n");

    while (1)
    {
        uint8_t data = uart2_read_char();

        if ((data == '\r') || (data == '\n'))
        {
            if (index > 0U)
            {
                cmd[index] = '\0';
                process_command(cmd);
                index = 0U;
            }
        }
        else if (index < (CMD_BUFFER_SIZE - 1U))
        {
            cmd[index++] = (char)data;
        }
        else
        {
            index = 0U;
            uart_write_string("ERROR: COMMAND_TOO_LONG\r\n");
        }
    }
}