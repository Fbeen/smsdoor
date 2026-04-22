#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"

#include "hardware.h"
#include "console.h"
#include "commands.h"
#include "util.h"

static line_buffer_t debug_line;

// ringbuffer
static console_entry_t log_buf[LOG_LINES];
static uint32_t log_head = 0;
static uint32_t log_next_id = 1;

// line buffer (voor printf fragments)
static char linebuf[LOG_LEN];
static int linepos = 0;

static void log_add_line(const char *line)
{
    console_entry_t *e = &log_buf[log_head];

    e->id = log_next_id++;

    strncpy(e->text, line, LOG_LEN - 1);
    e->text[LOG_LEN - 1] = '\0';

    log_head = (log_head + 1) % LOG_LINES;
}

void console_write(const char *buf, int len)
{
    for(int i = 0; i < len; i++)
    {
        char c = buf[i];

        // 1. altijd naar UART
        uart_putc(UART_DEBUG, c);

        // 2. line buffering
        if(c == '\n')
        {
            linebuf[linepos] = '\0';
            log_add_line(linebuf);
            linepos = 0;
        }
        else
        {
            if(linepos < LOG_LEN - 1)
                linebuf[linepos++] = c;
        }
    }
}

void cprintf(const char *fmt, ...)
{
    char tmp[256];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    if(len <= 0) return;
    if(len > sizeof(tmp)) len = sizeof(tmp);

    console_write(tmp, len);
}

int console_get_since(uint32_t last_id, console_entry_t *out, int max)
{
    int count = 0;

    for(int i = 0; i < LOG_LINES; i++)
    {
        console_entry_t *e = &log_buf[i];

        if(e->id > last_id)
        {
            out[count++] = *e;
            if(count >= max)
                break;
        }
    }

    return count;
}

void console_clear(void)
{
    log_head = 0;
    log_next_id = 1;
    linepos = 0;
}

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