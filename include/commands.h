#ifndef COMMANDS_H
#define COMMANDS_H

#include "config.h"

#define SRC_CONSOLE 1
#define SRC_SMS     2

/* command levels */
#define CMD_LEVEL_USER    0
#define CMD_LEVEL_ADMIN   1
#define CMD_LEVEL_CONSOLE 2

typedef enum
{
    CMD_UNKNOWN = 0,
    CMD_INIT,
    CMD_ADD,
    CMD_DEL,
    CMD_LIST,
    CMD_PROMOTE,
    CMD_DEMOTE,
    CMD_UP,
    CMD_DOWN,
    CMD_HELP,
    CMD_PIN,
    CMD_INFO,
    CMD_CLOSEAT,
    CMD_AT,
    CMD_LOG,
    CMD_OVERHEAD,
} command_id_t;

typedef struct
{
    command_id_t id;
    char args[COMMAND_SIZE];
    uint8_t source;
    char sender[PHONENR_SIZE];  // telefoonnummer, of leeg bij console
} command_t;

typedef void (*cmd_func_t)(command_t *, char *);

typedef struct
{
    command_id_t id;
    const char *names[6];
    uint8_t level;
    cmd_func_t func;
} command_entry_t;

command_t make_command(char *line, uint8_t source, const char *sender);
static void send_response(command_t *cmd, char *response);
bool        command_allowed(command_t *cmd, uint8_t level);
void        process_command(command_t *cmd);
static void cmd_init(command_t *cmd, char *response);
static void cmd_add(command_t *cmd, char *response);
static void cmd_del(command_t *cmd, char *response);
static void cmd_list(command_t *cmd, char *response);
static void cmd_promote(command_t *cmd, char *response);
static void cmd_demote(command_t *cmd, char *response);
static void cmd_up(command_t *cmd, char *response);
static void cmd_down(command_t *cmd, char *response);
static void cmd_overhead(command_t *cmd, char *response);
static void cmd_help(command_t *cmd, char *response);
static void cmd_pin(command_t *cmd, char *response);
static void cmd_info(command_t *cmd, char *response);
static void cmd_closeat(command_t *cmd, char *response);
static void cmd_at(command_t *cmd, char *response);
static void cmd_log(command_t *cmd, char *response);

#endif