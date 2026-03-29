#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "log.h"

static log_entry_t log_buffer[LOG_ENTRIES];
static int log_index = 0;
static int log_items = 0;

extern volatile dt_t clock_dt;

/* adds a log entry to the circular log buffer */
void log_add(const char *event, const char *args, const char *who, bool status)
{
    log_entry_t *e = &log_buffer[log_index];

    e->dt = clock_dt;

    if (event) strncpy(e->event, event, LOG_TEXT_SIZE - 1);
    else e->event[0] = 0;

    if (args) strncpy(e->args, args, LOG_TEXT_SIZE - 1);
    else e->args[0] = 0;

    if (who) strncpy(e->who, who, PHONENR_SIZE - 1);
    else e->who[0] = 0;

    e->event[LOG_TEXT_SIZE - 1] = 0;
    e->args[LOG_TEXT_SIZE - 1] = 0;
    e->who[PHONENR_SIZE - 1] = 0;

    e->status = status;

    log_index++;
    if (log_index >= LOG_ENTRIES)
        log_index = 0;

    if (log_items < LOG_ENTRIES)
        log_items++;
}

/* returns how many log entries are available */
int log_count(void)
{
    return log_items;
}

/* retrieves one log entry from a given index */
log_entry_t *log_get(int index)
{
    if (index >= log_items)
        return NULL;

    int i = log_index - log_items + index;

    if (i < 0)
        i += LOG_ENTRIES;

    return &log_buffer[i];
}

/* puts one log entry into a text rule. short format for sms or long for console */
void log_format_entry(log_entry_t *e, char *buf, int size, bool shortfmt)
{
    char args_buf[LOG_TEXT_SIZE];
    char who_buf[LOG_TEXT_SIZE];

    const char *args = e->args;
    const char *who  = e->who;

    /* Short format bewerking */
    if (shortfmt)
    {
        /* args inkorten als telefoonnummer */
        if (e->args[0] == '+')
        {
            phone_short(e->args, args_buf);
            args = args_buf;
        }

        /* who inkorten */
        if (strcmp(e->who, "console") == 0)
        {
            strcpy(who_buf, "cnsl");
            who = who_buf;
        }
        else if (e->who[0] == '+')
        {
            phone_short(e->who, who_buf);
            who = who_buf;
        }
    }

    int len = snprintf(buf, size,
                       "%02d-%02d %02d:%02d %s",
                       e->dt.day,
                       e->dt.month,
                       e->dt.hour,
                       e->dt.min,
                       e->event);

    if (args[0] != '\0' && len < size)
    {
        len += snprintf(buf + len, size - len, " %s", args);
    }

    if (who[0] != '\0' && len < size)
    {
        len += snprintf(buf + len, size - len, " %s", who);
    }

    if (len < size)
    {
        snprintf(buf + len, size - len, " %s",
                 e->status ? "OK" : "FOUT");
    }
}

/* returns last four digits from a telephone number */
void phone_short(const char *number, char *out)
{
    int len = strlen(number);

    if (len >= 4)
    {
        strcpy(out, &number[len - 4]);
    }
    else
    {
        strcpy(out, number);
    }
}
/*
void log_test() 
{
    log_add("INIT", "", "+31612345678", true);
    log_add("ADD", "+31612345678", "+31612345678", true);
    log_add("DEL", "+31612345678", "+31612345678", true);
    log_add("PROMOTE", "+31612345678", "+31612345678", true);
    log_add("DEMOTE", "+31612345678", "", true);
    log_add("OPEN", "", "+31612345678", true);
    log_add("CLOSE", "", "", true);
    log_add("PIN", "", "+31612345678", true);
    log_add("CLOSEAT 14:00", "", "+31612345678", true);
    log_add("PUK ASKED", "", "+31612345678", false);
    log_add("PIN WRONG", "", "+31612345678", false);
}
*/