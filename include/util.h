#ifndef UTIL_H
#define UTIL_H

#include "config.h"

typedef struct
{
    char buffer[LINE_BUFFER_SIZE];
    int index;
} line_buffer_t;

void str_to_upper(char *s);
void str_trim(char *s);

#endif