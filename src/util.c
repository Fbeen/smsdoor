#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "pico/stdlib.h"

void str_to_upper(char *s)
{
    while (*s)
    {
        *s = toupper((unsigned char)*s);
        s++;
    }
}

void str_trim(char *s)
{
    int len = strlen(s);

    while (len > 0 && (s[len-1] == '\r' || s[len-1] == '\n' || s[len-1] == ' '))
    {
        s[len-1] = 0;
        len--;
    }
}

void get_uptime_string(char *buf)
{
    uint32_t sec = to_ms_since_boot(get_absolute_time()) / 1000;

    uint32_t days = sec / 86400;
    uint32_t hours = (sec % 86400) / 3600;
    uint32_t minutes = (sec % 3600) / 60;

    if (sec < 3600)
    {
        sprintf(buf, "%lu minute%s",
                minutes,
                (minutes == 1) ? "" : "s");
    }
    else if (sec < 86400)
    {
        sprintf(buf, "%lu h. %lu m.", hours, minutes);
    }
    else
    {
        sprintf(buf, "%lu day%s %lu h.",
                days,
                (days == 1) ? "" : "s",
                hours);
    }
}