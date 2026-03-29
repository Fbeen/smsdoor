#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "clock.h"

#define LOG_TEXT_SIZE   32
#define LOG_ENTRIES     32     /* aantal log regels in RAM */

typedef struct
{
    dt_t dt;                    /* datum + tijd */
    char event[LOG_TEXT_SIZE];  /* gebeurtenis */
    char args[LOG_TEXT_SIZE];   /* argumenten */
    char who[PHONENR_SIZE];     /* nummer / auto / unknown */
    bool status;                /* true = OK, false = FOUT */

} log_entry_t;

void log_init(void);
void log_add(const char *event, const char *args, const char *who, bool status);

int  log_count(void);
log_entry_t *log_get(int index);
void log_format_entry(log_entry_t *e, char *buf, int size, bool shortfmt);
void phone_short(const char *number, char *out);

void log_test();

#endif