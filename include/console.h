#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

#define LOG_LINES  50
#define LOG_LEN    128

typedef struct {
    uint32_t id;
    char text[LOG_LEN];
} console_entry_t;

void cprintf(const char *fmt, ...);

/* ophalen voor web */
int console_get_since(uint32_t last_id, console_entry_t *out, int max);
void console_uart_task();

#endif