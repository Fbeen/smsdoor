#include <string.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"

#include "hardware.h"
#include "console.h"
#include "commands.h"
#include "util.h"

static line_buffer_t debug_line;

void console_uart_task(void)
{
    char response[MAX_CMD_LEN];
    command_t cmd;

    while (uart_is_readable(UART_DEBUG))
    {
        char c = uart_getc(UART_DEBUG);

        if (c == '\r' || c == '\n')
        {
            if (debug_line.index > 0)
            {
                debug_line.buffer[debug_line.index] = 0;

                cmd = make_command(debug_line.buffer, SRC_CONSOLE, "console");
                process_command(&cmd, response);

                debug_line.index = 0;
            }
        }
        else
        {
            if (debug_line.index < LINE_BUFFER_SIZE - 1)
            {
                debug_line.buffer[debug_line.index++] = c;
            }
        }
    }
}