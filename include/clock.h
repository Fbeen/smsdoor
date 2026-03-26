#ifndef CLOCK_H
#define CLOCK_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

char *datetime_to_string(struct tm *t, char *out, int maxlen);
bool get_time_from_modem(struct tm *t);
bool parse_time(const char *s, datetime_t *dt);
uint32_t time_to_seconds(datetime_t *dt);
void shutter_task();
void clock_tick();
void clock_task();

#endif