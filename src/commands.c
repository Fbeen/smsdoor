#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "util.h"
#include "phonebook.h"
#include "modem.h"
#include "commands.h"

static const command_entry_t command_table[] =
{
    { CMD_TEST,   { "TEST", NULL }, CMD_LEVEL_USER,  cmd_test },
    { CMD_INIT,   { "INIT", NULL }, CMD_LEVEL_ADMIN, cmd_init },
    { CMD_ADD,    { "ADD",  NULL }, CMD_LEVEL_ADMIN, cmd_add },
    { CMD_DEL,    { "DEL",  NULL }, CMD_LEVEL_ADMIN, cmd_del },
    { CMD_LIST,   { "LIST", NULL }, CMD_LEVEL_ADMIN, cmd_list },
    { CMD_PROMOTE,{ "PROMOTE", NULL }, CMD_LEVEL_ADMIN, cmd_promote },
    { CMD_DEMOTE, { "DEMOTE",  NULL }, CMD_LEVEL_ADMIN, cmd_demote },
    { CMD_UP,     { "UP", "OPEN", "OMHOOG", "OP", NULL }, CMD_LEVEL_USER, cmd_up },
    { CMD_DOWN,   { "DOWN", "DICHT", "BENEDEN", "CLOSE", "NEER", NULL }, CMD_LEVEL_USER, cmd_down },
    { CMD_HELP,   { "HELP", NULL }, CMD_LEVEL_ADMIN, cmd_help },
    { CMD_PIN,    { "PIN", NULL }, CMD_LEVEL_CONSOLE, cmd_pin },
    { CMD_INFO,   { "INFO", NULL }, CMD_LEVEL_ADMIN, cmd_info },
};

#define CMD_TABLE_SIZE (sizeof(command_table) / sizeof(command_table[0]))

static command_id_t find_command_id_by_name(const char *name)
{
    for (int i = 0; i < CMD_TABLE_SIZE; i++)
    {
        const command_entry_t *entry = &command_table[i];

        for (int j = 0; entry->names[j] != NULL; j++)
        {
            if (strcmp(entry->names[j], name) == 0)
            {
                return entry->id;
            }
        }
    }

    return CMD_UNKNOWN;
}

static const command_entry_t *command_find_by_id(command_id_t id)
{
    for (int i = 0; i < CMD_TABLE_SIZE; i++)
    {
        if (command_table[i].id == id)
        {
            return &command_table[i];
        }
    }

    return NULL;
}

command_t make_command(char *line, uint8_t source, const char *sender)
{
    command_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    str_trim(line);

    char *space = strchr(line, ' ');

    char command_str[COMMAND_SIZE] = {0};

    if (space)
    {
        *space = 0;

        strncpy(command_str, line, sizeof(command_str) - 1);
        strncpy(cmd.args, space + 1, sizeof(cmd.args) - 1);
    }
    else
    {
        strncpy(command_str, line, sizeof(command_str) - 1);
    }

    str_trim(command_str);
    str_to_upper(command_str);

    cmd.id = find_command_id_by_name(command_str);

    cmd.source = source;

    if (sender)
    {
        strncpy(cmd.sender, sender, sizeof(cmd.sender) - 1);
    }

    return cmd;
}

static void send_response(command_t *cmd, char *response)
{
    printf("%s\n\n", response);

    /* Als SMS → stuur antwoord terug */
    if (strlen(cmd->sender) > 0)
    {
        modem_send_sms(cmd->sender, response);
    }
}

bool command_allowed(command_t *cmd, uint8_t level)
{
    /* commands from console are always approved */
    if (cmd->source == SRC_CONSOLE)
        return true;

    /* command comes from sms */

    /* deny 'console only' commands */
    if (level == CMD_LEVEL_CONSOLE)
        return false;

    /* deny unknown senders */
    if (strlen(cmd->sender) == 0)
        return false;

    /* deny senders that are not in the phonebook */
    if (!phonebook_exists(cmd->sender))
        return false;

    /* deny 'admin only' commands for normal users */
    if (level == CMD_LEVEL_ADMIN)
    {
        if (!phonebook_is_admin(cmd->sender))
            return false;
    }

    return true;
}

void process_command(command_t *cmd)
{
    char response[160];

    const command_entry_t *entry = command_find_by_id(cmd->id);

    if (!entry)
    {
        sprintf(response, "Unknown command");
        send_response(cmd, response);
        return;
    }

    if (!command_allowed(cmd, entry->level))
    {
        sprintf(response, "Permission denied");
        send_response(cmd, response);
        return;
    }

    sprintf(response, "%s:\n", entry->names[0]);

    entry->func(cmd, response);

    send_response(cmd, response);
}

static void cmd_test(command_t *cmd, char *response)
{
    strcat(response, "TEST command received!");
}

static void cmd_init(command_t *cmd, char *response)
{
    int err;

    if (cmd->source != SRC_SMS)
    {
        strcat(response, "INIT only via SMS");
    }
    else if (phonebook_count() != 0)
    {
        strcat(response, "Already initialized");
    }
    else
    {
        err = phonebook_add(cmd->sender);
        if(err == PB_OK)
            strcat(response, "Phonebook initialized.\nYour number has been added.");
        else
            strcat(response, phonebook_strerror(err));
    }
}

static void cmd_add(command_t *cmd, char *response)
{
    int err = phonebook_add(cmd->args);

    if (err == PB_OK)
        strcat(response, "Number added");
    else
        strcat(response, phonebook_strerror(err));
}

static void cmd_del(command_t *cmd, char *response)
{
    char number[PHONENR_SIZE];
    char sender[PHONENR_SIZE];

    if (!phone_normalize(number, cmd->args))
    {
        strcat(response, "Invalid number");
        return;
    }

    phone_normalize(sender, cmd->sender);

    /* SMS: jezelf verwijderen mag niet */
    if (cmd->source == SRC_SMS && strcmp(number, sender) == 0)
    {
        strcat(response, "Cannot delete own number");
        return;
    }

    /* Als admin verwijderd wordt, check of laatste admin */
    if (phonebook_is_admin(number))
    {
        if (phonebook_count_admins() <= 1)
        {
            strcat(response, "Cannot delete last admin");
            return;
        }
    }

    int r = phonebook_remove(number);
    if (r == PB_OK)
        strcat(response, "Number removed");
    else
        strcat(response, phonebook_strerror(r));
}

static void cmd_list(command_t *cmd, char *response)
{
    phonebook_entry_t entry;
    int count = phonebook_count();

    if (count == 0)
    {
        strcat(response, "Phonebook empty");
    }
    else
    {
        for (int i = 0; i < MAX_PHONES; i++)
        {
            if (phonebook_get(i, &entry))
            {
                strcat(response, entry.number);
                if(entry.isAdmin)
                    strcat(response, " *");
                strcat(response, "\n");
            }
        }

        char tmp[32];
        sprintf(tmp, "Total numbers: %d.", count);
        strcat(response, tmp);
    }
}

static void cmd_promote(command_t *cmd, char *response)
{
    char number[PHONENR_SIZE];
    char sender[PHONENR_SIZE];

    if (!phone_normalize(number, cmd->args))
    {
        strcat(response, "Invalid number");
        return;
    }

    if(phonebook_is_admin(number)) {
        strcat(response, "User is already an admin.");
        return;
    }

    int r = phonebook_set_admin(number, 1);
    if(r == PB_OK)
        strcat(response, "User promoted to admin!");
    else
        strcat(response, phonebook_strerror(r));
}

static void cmd_demote(command_t *cmd, char *response)
{
    char number[PHONENR_SIZE];
    char sender[PHONENR_SIZE];

    if (!phone_normalize(number, cmd->args))
    {
        strcat(response, "Invalid number");
        return;
    }

    phone_normalize(sender, cmd->sender);

    /* SMS: jezelf niet demoten */
    if (cmd->source == SRC_SMS && strcmp(number, sender) == 0)
    {
        strcat(response, "Cannot demote yourself");
        return;
    }

    if (!phonebook_is_admin(number))
    {
        strcat(response, "User is already normal user");
        return;
    }

    if (phonebook_count_admins() <= 1)
    {
        strcat(response, "Cannot demote last admin");
        return;
    }

    int r = phonebook_set_admin(number, 0);
    if (r == PB_OK)
        strcat(response, "User demoted");
    else
        strcat(response, phonebook_strerror(r));
}

static void cmd_up(command_t *cmd, char *response)
{
    strcat(response, "Rolluik gaat omhoog");
}

static void cmd_down(command_t *cmd, char *response)
{
    strcat(response, "Rolluik gaat omlaag");
}

static void cmd_help(command_t *cmd, char *response)
{
    const char help_text[] =
    "door:\n"
    "UP\n"
    "DOWN\n"
    "CLOSEAT <hh:mm>\n"
    "CLOSEAT OFF\n"
    "\n"
    "users:\n"
    "ADD <nr>\n"
    "DEL <nr>\n"
    "LIST\n"
    "\n"
    "admin:\n"
    "PROMOTE <nr>\n"
    "DEMOTE <nr>\n"
    "\n"
    "system:\n"
    "PIN <code> *\n"
    "INFO\n"
    "LOG\n"
    "\n"
    "* console only\n";

    strcat(response, help_text);
}

static void cmd_pin(command_t *cmd, char *response)
{
    const char *pin = cmd->args;
    int len = strlen(pin);

    /* Alleen via console */
    if (cmd->source != SRC_CONSOLE)
    {
        strcat(response, "Console only");
        return;
    }

    /* Lengte check */
    if (len < 4 || len > 6)
    {
        strcat(response, "PIN must be 4-6 digits");
        return;
    }

    /* Alleen cijfers */
    for (int i = 0; i < len; i++)
    {
        if (!isdigit((unsigned char)pin[i]))
        {
            strcat(response, "PIN must be numeric");
            return;
        }
    }

    /* Opslaan in flash */
    config_set_pin(pin);

    strcat(response, "SIM PIN stored, Rebooting...");

    send_response(cmd, response);

    modem_send("AT+CFUN=1,1"); // Full modem reset

    sleep_ms(3000); // wait a three seconds

    watchdog_reboot(0, 0, 0); // reset pico
}

static void cmd_info(command_t *cmd, char *response)
{
    char uptime[32];
    char datetime[32];


    get_uptime_string(uptime);

    strcat(response, "SMSDOOR v");
    strcat(response, VERSION);
    strcat(response, "\n");
    
    strcat(response, "Uptime: ");
    strcat(response, uptime);
    strcat(response, "\n");

    if (modem_get_time(datetime))
    {
        strcat(response, "System time: ");
        strcat(response, datetime);
        strcat(response, "\n");
    }

    char buf[32];
    sprintf(buf, "Users %d Admin %d\n",
            phonebook_count(),
            phonebook_count_admins());

    strcat(response, buf);
}
