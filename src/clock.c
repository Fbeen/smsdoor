#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "clock.h"
#include "modem.h"
#include "config.h"
#include "rshutter.h"
#include "log.h"

/* ----------------------------------------------------------
   Global clock
   ---------------------------------------------------------- */

volatile dt_t clock_dt =
{
    .year = 0,
    .month = 0,
    .day = 0,
    .hour = 0,
    .min = 0,
    .sec = 0,
    .synced = false
};

/* ----------------------------------------------------------
   Helpers
   ---------------------------------------------------------- */

static int days_in_month(int month, int year)
{
    if (month == 2)
    {
        if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
            return 29;
        return 28;
    }

    if (month == 4 || month == 6 || month == 9 || month == 11)
        return 30;

    return 31;
}

/* ----------------------------------------------------------
   Get datetime string
   ---------------------------------------------------------- */

bool clock_get_time(char *buf)
{
    if(!clock_dt.synced)
        return false;

    sprintf(buf, "%02u-%02u-%04u %02u:%02u:%02u",
            clock_dt.day,
            clock_dt.month,
            clock_dt.year,
            clock_dt.hour,
            clock_dt.min,
            clock_dt.sec);

    return true;
}

/* ----------------------------------------------------------
   sets the time one second later
   ---------------------------------------------------------- */

static void clock_add_second(void)
{
    clock_dt.sec++;

    if(clock_dt.sec >= 60)
    {
        clock_dt.sec = 0;
        clock_dt.min++;

        if(clock_dt.min >= 60)
        {
            clock_dt.min = 0;
            clock_dt.hour++;

            if(clock_dt.hour >= 24)
            {
                clock_dt.hour = 0;
                clock_dt.day++;

                if(clock_dt.day > days_in_month(clock_dt.month, clock_dt.year))
                {
                    clock_dt.day = 1;
                    clock_dt.month++;

                    if(clock_dt.month > 12)
                    {
                        clock_dt.month = 1;
                        clock_dt.year++;
                    }
                }
            }
        }
    }
}

/* ----------------------------------------------------------
   Set clock from modem string (+CCLK or +CTZV)
   ---------------------------------------------------------- */

void clock_set_clock(char *buf)
{
    int yy, MM, dd, hh, mm, ss;

    if (strstr(buf, "+CCLK:"))
    {
        if (sscanf(buf, "+CCLK: \"%2d/%2d/%2d,%2d:%2d:%2d",
                   &yy, &MM, &dd, &hh, &mm, &ss) != 6)
            return;
    }
    else if (strstr(buf, "+CTZV:"))
    {
        if (sscanf(buf, "+CTZV: %*[^,],%2d/%2d/%2d,%2d:%2d:%2d",
                   &yy, &MM, &dd, &hh, &mm, &ss) != 6)
            return;
    }
    else
        return;

    /* ignore invalid modem clock */
    if (yy == 70)
    {
        printf("Ignoring invalid modem time\n");
        return;
    }

    clock_dt.year  = 2000 + yy;
    clock_dt.month = MM;
    clock_dt.day   = dd;
    clock_dt.hour  = hh;
    clock_dt.min   = mm;
    clock_dt.sec   = ss;

    clock_dt.synced = true;

    printf("Clock synced: %02d-%02d-%04d %02d:%02d:%02d\n",
           dd, MM, clock_dt.year, hh, mm, ss);
}

/* ----------------------------------------------------------
   Called from ISR (10x per second)
   ---------------------------------------------------------- */

void clock_tick()
{
    static uint16_t div = 0;
    div += ISR_REPEAT_MS;

    if(div >= 1000)
    {
        div = 0;
        clock_add_second();

        /* elke dag om 03:01 opnieuw sync */
        if(clock_dt.hour == 3 && clock_dt.min == 1 && clock_dt.sec == 0)
        {
            clock_dt.synced = false;
        }
    }
}

/* ----------------------------------------------------------
   Clock task (main loop)
   ---------------------------------------------------------- */

void clock_task()
{
    static uint16_t last_min = 0xFFFF;
    static uint8_t first_try = 2;
    uint16_t current_min = clock_dt.hour * 60 + clock_dt.min;

    /* once per minute */
    if(current_min != last_min)
    {
        last_min = current_min;

        if(clock_dt.synced)
        {
            /* automatic closing time */
            if(current_min == cfg.close_time)
            {
                rshutter_down();
                overhead_down();
                printf("clock task: close now.\n");
                log_add("AUTO CLOSE", "", "", true);
            }
        }
        else
        {
            /* skip first run as modem still might be busy */
            if(first_try > 0)
                first_try--;
            else {
                /* try to get time from network */
                modem_send("AT+CCLK?");
            }
        }
    }
}