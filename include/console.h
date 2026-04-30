#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

#define LOG_LINES  50
#define LOG_LEN    128

/* console output to defines */
#define OUT_CONSOLE (1 << 0)
#define OUT_WIFI    (1 << 1)
#define OUT_BOTH    (OUT_CONSOLE | OUT_WIFI)

typedef struct {
    uint32_t id;
    char text[LOG_LEN];
} console_entry_t;

void csprintf(uint8_t out, const char *fmt, ...);
void cprintf(const char *fmt, ...);

/* ophalen voor web */
uint32_t console_last_id(void);
int console_get_since(uint32_t last_id, console_entry_t *out, int max);
void console_uart_task();

#endif