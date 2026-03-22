#ifndef COMMANDS_H
#define COMMANDS_H

#include "config.h"

#define SRC_CONSOLE 1
#define SRC_SMS     2

typedef struct
{
    char command[COMMAND_SIZE];
    char args[COMMAND_SIZE];
    uint8_t source;
    char sender[PHONENR_SIZE];   // telefoonnummer, of leeg bij console
} command_t;

command_t make_command(char *line, uint8_t source, const char *sender);
bool      command_allowed(command_t *cmd);
void      process_command(command_t *cmd);

#endif