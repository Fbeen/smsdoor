#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint16_t year;
    uint8_t  month;
    uint8_t  day;

    uint8_t  hour;
    uint8_t  min;
    uint8_t  sec;

    bool synced;
} dt_t;

bool clock_get_time(char *timestring);
void clock_set_clock(char *buf);
void clock_tick();
void clock_task();

#endif