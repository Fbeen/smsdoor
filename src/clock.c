#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "clock.h"
#include "modem.h"
#include "config.h"
#include "rshutter.h"

volatile uint32_t seconds_today = 0;
volatile bool clock_synced = false;
volatile uint32_t last_sync_seconds = 0;

char *datetime_to_string(struct tm *t, char *out, int maxlen)
{
    strftime(out, maxlen, "%Y-%m-%d %H:%M:%S", t);
}

bool get_time_from_modem(struct tm *t)
{
    char resp[256];

    if (!modem_command("AT+CCLK?", resp, sizeof(resp), 2000))
        return false;

    char *p = strstr(resp, "+CCLK:");
    if (!p)
        return false;

    char *q1 = strchr(p, '"');
    if (!q1)
        return false;

    char *q2 = strchr(q1 + 1, '"');
    if (!q2)
        return false;

    char temp[32];
    int len = q2 - q1 - 1;
    if (len <= 0 || len >= sizeof(temp))
        return false;

    strncpy(temp, q1 + 1, len);
    temp[len] = 0;

    // timezone verwijderen
    char *tz = strchr(temp, '+');
    if (tz) *tz = 0;

    // temp = 25/03/24,21:43:12

    if(strlen(temp) < 17)
        return false;

    int year  = (temp[0]-'0')*10 + (temp[1]-'0');
    int month = (temp[3]-'0')*10 + (temp[4]-'0');
    int day   = (temp[6]-'0')*10 + (temp[7]-'0');

    int hour  = (temp[9]-'0')*10 + (temp[10]-'0');
    int min   = (temp[12]-'0')*10 + (temp[13]-'0');
    int sec   = (temp[15]-'0')*10 + (temp[16]-'0');

    if ((2000 + year) < 2025)
        return false;

    t->tm_year = year + 100; // sinds 1900
    t->tm_mon  = month - 1;
    t->tm_mday = day;
    t->tm_hour = hour;
    t->tm_min  = min;
    t->tm_sec  = sec;

    return true;
}

uint32_t tm_to_seconds(struct tm *t)
{
    return t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
}

/* called 10 times per second by ISR */
void clock_tick()
{
    static uint8_t div = 0;
    div++;

    if(div >= 10)
    {
        div = 0;
        seconds_today++;

        /* at midnight */
        if(seconds_today >= 86400)
        {
            seconds_today = 0;
            clock_synced = false;   // nieuwe dag → opnieuw netwerktijd ophalen
        }
    }
}

/* called from main loop */
void clock_task()
{
    struct tm nettime;
    static uint16_t last_min = 0xFFFF;
    uint16_t current_min = seconds_today / 60;

    /* once per minute */
    if(current_min != last_min)
    {
        last_min = current_min;

        /* when clock is not synced */
        if(clock_synced)
        {
            /* automatic closing time */
            if(current_min == cfg.close_time)
            {
                rshutter_down();
                printf("clock task: close now.\n");
            }

        } else {

            /* try to get time from network */
            if (get_time_from_modem(&nettime))
            {
                seconds_today = tm_to_seconds(&nettime);
                clock_synced = true;
                printf("Clock set. Current time %d:%02d\n\n", nettime.tm_hour, nettime.tm_min);
            }
         }
    }
}
