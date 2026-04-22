#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "util.h"
#include "phonebook.h"
#include "modem.h"
#include "commands.h"
#include "clock.h"
#include "rshutter.h"
#include "log.h"
#include "util.h"
#include "hardware.h"
#include "led.h"
#include "console.h"

static const command_entry_t command_table[] =
{
    { CMD_INIT,    { "INIT", NULL }, CMD_LEVEL_ADMIN, cmd_init },
    { CMD_ADD,     { "ADD",  NULL }, CMD_LEVEL_ADMIN, cmd_add },
    { CMD_DEL,     { "DEL",  NULL }, CMD_LEVEL_ADMIN, cmd_del },
    { CMD_LIST,    { "LIST", NULL }, CMD_LEVEL_ADMIN, cmd_list },
    { CMD_PROMOTE, { "PROMOTE", NULL }, CMD_LEVEL_ADMIN, cmd_promote },
    { CMD_DEMOTE,  { "DEMOTE",  NULL }, CMD_LEVEL_ADMIN, cmd_demote },
    { CMD_UP,      { "UP", "OPEN", "OMHOOG", "OP", NULL }, CMD_LEVEL_USER, cmd_up },
    { CMD_DOWN,    { "DOWN", "DICHT", "OMLAAG", "CLOSE", "NEER", NULL }, CMD_LEVEL_USER, cmd_down },
    { CMD_OVERHEAD,{ "OVERHEAD", "GARAGE", NULL }, CMD_LEVEL_ADMIN, cmd_overhead },
    { CMD_HELP,    { "HELP", NULL }, CMD_LEVEL_ADMIN, cmd_help },
    { CMD_PIN,     { "PIN", NULL }, CMD_LEVEL_CONSOLE, cmd_pin },
    { CMD_SSID,    { "SSID", NULL }, CMD_LEVEL_ADMIN, cmd_ssid },
    { CMD_PASS,    { "PASS", NULL }, CMD_LEVEL_ADMIN, cmd_pass },
    { CMD_INFO,    { "INFO", NULL }, CMD_LEVEL_ADMIN, cmd_info },
    { CMD_CLOSEAT, { "CLOSEAT", NULL }, CMD_LEVEL_ADMIN, cmd_closeat },
    { CMD_AT,      { "AT", "MODEM", NULL }, CMD_LEVEL_CONSOLE, cmd_at },
    { CMD_LOG,     { "LOG", NULL }, CMD_LEVEL_ADMIN, cmd_log },
};

#define CMD_TABLE_SIZE (sizeof(command_table) / sizeof(command_table[0]))

/* ----------------------------------------------------------
   Helpers
   ---------------------------------------------------------- */

/* find the right command by name e.g. "UP" */
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

/* find the right command by id e.g. CMD_UP */
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

/* fills a command_t structure wih info from the commandline or sms message */
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

/* sends a response from a command function to the console and optionally to a sms */
static void send_response(command_t *cmd, char *response)
{
    if(strlen(response) == 0)
        return;
        
    cprintf("[TC] %s\n", response);

    /* Als SMS → stuur antwoord terug */
    if (cmd->source == SRC_SMS)
    {
        modem_send_sms(cmd->sender, response);
    }
}

/* this is the firewall to keep unauthorized people away */
bool command_allowed(command_t *cmd, uint8_t level)
{
    /* Init is a very special command and has his onwn firewall */
    if(cmd->id == CMD_INIT)
        return true;
        
    /* commands from console or web are always approved */
    if (cmd->source == SRC_CONSOLE || cmd->source == SRC_WEB)
        return true;

    /* command comes from sms */

    /* deny 'console only' commands */
    if (level == CMD_LEVEL_CONSOLE) {
        led_activate(GPIO_LED_ERROR, 200, 600);
        return false;
    }

    /* deny unknown senders */
    if (strlen(cmd->sender) == 0) {
        led_activate(GPIO_LED_ERROR, 200, 600);
        return false;
    }

    /* deny senders that are not in the phonebook */
    if (!phonebook_exists(cmd->sender)) {
        led_activate(GPIO_LED_ERROR, 200, 600);
        return false;
    }

    /* deny 'admin only' commands for normal users */
    if (level == CMD_LEVEL_ADMIN)
    {
        if (!phonebook_is_admin(cmd->sender)) {
            led_activate(GPIO_LED_ERROR, 200, 600);
            return false;
        }
    }

    return true;
}

/* processes a command that is received from sms or console */
void process_command(command_t *cmd, char *response)
{
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

/* ----------------------------------------------------------
   COMMAND handlers
   ---------------------------------------------------------- */

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
        if(err == PB_OK) {
            strcat(response, "Phonebook initialized.\nYour number has been added. Send HELP to learn more commands");
            log_add("INIT", "", cmd->sender, true);
        } else {
            strcat(response, phonebook_strerror(err));
            log_add("INIT", "", cmd->sender, false);
        }
    }
}

static void cmd_add(command_t *cmd, char *response)
{
    int err = exec_cmd_add(cmd->args, cmd->sender);

    if (err == PB_OK)
    {
        strcat(response, "Number added");

        return;
    }
    
    strcat(response, phonebook_strerror(err));
}

static void cmd_del(command_t *cmd, char *response)
{
    char number[PHONENR_SIZE];
    char sender[PHONENR_SIZE];

    phone_normalize(sender, cmd->sender);

    /* SMS: jezelf verwijderen mag niet */
    if (cmd->source == SRC_SMS && strcmp(number, sender) == 0)
    {
        strcat(response, "Cannot delete own number");
        return;
    }

    int err = exec_cmd_del(number, sender);

    log_add("DEL", number, cmd->sender, err == PB_OK);

    if (err == PB_OK) {
        strcat(response, "Number removed");
    } else {
        strcat(response, phonebook_strerror(err));
    }
}

static void cmd_list(command_t *cmd, char *response)
{
    phonebook_entry_t entry;
    int count = phonebook_count();

    if (cmd->source == SRC_CONSOLE) {
        strcpy(response, "\nLIST:\n\n");
    }

    if (count == 0)
    {
        strcat(response, "Phonebook empty\n\n");
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

    if (cmd->source == SRC_CONSOLE) {
        printf("%s\n\n", response);
        response[0] = 0;
    }

}

static void cmd_promote(command_t *cmd, char *response)
{
    int result = exec_cmd_promote(cmd->args, cmd->sender);

    if(result == PB_OK) {
        strcat(response, "User promoted to admin!");
    } else {
        strcat(response, phonebook_strerror(result));
    }
}

static void cmd_demote(command_t *cmd, char *response)
{
    int result = exec_cmd_demote(cmd->args, cmd->sender);

    if(result == PB_OK) {
        strcat(response, "User demoted");
    } else {
        strcat(response, phonebook_strerror(result));
    }
}

static void cmd_up(command_t *cmd, char *response)
{
    rshutter_up();
    log_add("OPEN", "", cmd->sender, true);
    strcpy(response, "Rolluik gaat omhoog");
}

static void cmd_down(command_t *cmd, char *response)
{
    rshutter_down();
    log_add("CLOSE", "", cmd->sender, true);
    strcpy(response, "Rolluik gaat omlaag");
}

static void cmd_overhead(command_t *cmd, char *response)
{
    const char *close_words[] ={"DOWN", "DICHT", "OMLAAG", "CLOSE", "NEER"};
    #define CLOSE_WORDS_COUNT (sizeof(close_words) / sizeof(close_words[0]))

    str_trim(cmd->args);
    str_to_upper(cmd->args);

    for (int i = 0; i < CLOSE_WORDS_COUNT; i++)
    {
        if (strcmp(cmd->args, close_words[i]) == 0)
        {
            overhead_down();
            log_add("OVERHEAD", "", cmd->sender, true);
            strcpy(response, "Overheaddeur gaat dicht");

            return;
        }
    }
    
    strcpy(response, "Unknown command");
}

static void cmd_help(command_t *cmd, char *response)
{
    if (cmd->source == SRC_SMS)
    {
        const char help_text[] =
        "door:\n"
        "UP\n"
        "DOWN\n"
        "OVERHEAD DOWN\n"
        "CLOSEAT <hh:mm>/OFF\n"
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
        "INFO\n"
        "LOG\n"
        "SIDD <name>\n"
        "PASS <passwd>";

        strcpy(response, help_text);
        return;
    }
    printf("\nHELP:\n"
        "\n"
        "[door]\n"
        "UP               Rolluik omhoog.\n"
        "DOWN             Rolluik omlaag.\n"
        "OVERHEAD DOWN    Overheaddeur dicht.\n"
        "CLOSEAT <hh:mm>  Zet de dagelijkse automatische sluitingstijd.\n"
        "CLOSEAT OFF      Zet de dagelijkse automatische sluitingstijd uit.\n"
        "\n"
        "[users]\n"
        "ADD <telnr>      Voeg een gebruiker toe.\n"
        "DEL <telnr>      Verwijder een gebruiker.\n"
        "LIST             Geef een overzicht van de gebruikers.\n"
        "\n"
        "[admin]\n"
        "PROMOTE <telnr>  Maak van een bestaande gebruiker een administrator.\n"
        "DEMOTE <telnr>   Maak van een bestaande administrator een normale gebruiker.\n"
        "\n"
        "[system]\n"
        "PIN <code> *     Stel een nieuwe pincode in om de SIM kaart te ontgrendelen.\n"
        "SIDD <name>      Stel een nieuwe SSID in voor het wifi AP.\n"
        "PASS <passwd>    Stel een nieuw wachtwoord in voor het wifi AP.\n"
        "AT <command> *   Stuur een commando rechtstreeks naar de A7670E modem.\n"
        "INFO             Vraag systeem informatie op.\n"
        "LOG              Laat de laatste gebeurtenissen zien.\n"
        "\n"
        "* console only\n\n");

        /* overwrite response */
        strcpy(response, "");
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
    config_save();

    log_add("PIN", "", cmd->sender, true);

    strcat(response, "SIM PIN stored, Rebooting...");

    send_response(cmd, response);

    modem_send("AT+CFUN=1,1"); // Full modem reset

    sleep_ms(3000); // wait a three seconds

    watchdog_reboot(0, 0, 0); // reset pico
}

static void cmd_ssid(command_t *cmd, char *response)
{
    const char *ssid = cmd->args;
    int len = strlen(ssid);

    /* Lengte check */
    if (len < 2 || len > 32)
    {
        strcat(response, "SSID must be 2-32 characters long");
        return;
    }

    /* Opslaan in flash */
    config_set_ssid(ssid);
    config_save();

    log_add("SSID", "", cmd->sender, true);

    strcat(response, "SSID stored, Rebooting...");

    send_response(cmd, response);

    watchdog_reboot(0, 0, 0); // reset pico
}

static void cmd_pass(command_t *cmd, char *response)
{
    const char *pass = cmd->args;
    int len = strlen(pass);

    /* Lengte check */
    if (len < 8 || len > 63)
    {
        strcat(response, "PASSWORD must be 8-63 characters long");
        return;
    }

    /* Opslaan in flash */
    config_set_pass(pass);
    config_save();

    log_add("PASSWORD", "", cmd->sender, true);

    strcat(response, "PASSWORD stored, Rebooting...");

    send_response(cmd, response);

    watchdog_reboot(0, 0, 0); // reset pico
}

static void cmd_info(command_t *cmd, char *response)
{
    char lines[INFO_LINES][INFO_LINE_LEN];
    int i;

    /* vul array met regels */
    exec_cmd_info(lines);

    /* begin met lege string */
    response[0] = 0;

    /* alle regels samenvoegen met \n */
    for (i = 0; i < INFO_LINES; i++)
    {
        strcat(response, lines[i]);

        if (i < (INFO_LINES - 1))
            strcat(response, "\n");
    }
    
    if (cmd->source == SRC_CONSOLE) {
        printf("\nINFO:\n\n");
        printf(response);
        response[0] = 0;
    } 
}

static void cmd_closeat(command_t *cmd, char *response)
{
    char tempbuf[32];

    str_to_upper(cmd->args);

    if (strcmp(cmd->args, "OFF") == 0)
    {
        cfg.close_time = CLOSE_DISABLED;
        config_save(&cfg);
        strcat(response, "Auto close disabled");
        log_add("CLOSEAT OFF", "", cmd->sender, true);
        return;
    }

    int h, m;

    if (sscanf(cmd->args, "%d:%d", &h, &m) != 2)
    {
        strcat(response, "Invalid time");
        log_add("CLOSEAT OFF", "", cmd->sender, false);
        return;
    }

    if (h < 0 || h > 23 || m < 0 || m > 59)
    {
        strcat(response, "Invalid time");
        log_add("CLOSEAT OFF", "", cmd->sender, false);
        return;
    }

    cfg.close_time = h * 60 + m;
    config_save();

    sprintf(tempbuf, "Auto close set to %d:%02d", h, m);
    strcat(response, tempbuf);

    sprintf(tempbuf, "CLOSEAT %d:%02d", h, m);
    log_add(tempbuf, "", cmd->sender, true);
}

static void cmd_at(command_t *cmd, char *response)
{
    if(strlen(cmd->args) == 0)
    {
        strcat(response, "Usage: AT <modem command>\n");
        return;
    }

    modem_send(cmd->args);
    strcat(response, "Sent to modem\n");
}

static void cmd_log(command_t *cmd, char *response)
{
    char line[64];

    /* SMS → zoveel mogelijk logregels binnen 160 chars */
    if (cmd->source == SRC_SMS)
    {
        int count = log_count();
        if (count == 0)
        {
            strcpy(response, "Log empty");
            return;
        }

        response[0] = '\0';
        int total_len = 0;

        /* begin bij laatste en werk terug */
        for (int i = count - 1; i >= 0; i--)
        {
            log_entry_t *e = log_get(i);
            log_format_entry(e, line, sizeof(line), true);

            int line_len = strlen(line);

            /* +1 voor newline */
            if (total_len + line_len + 1 >= 160)
                break;

            strcat(response, line);
            strcat(response, "\n");

            total_len += line_len + 1;
        }

        return;
    }

    /* Console → hele log */
    int count = log_count();

    strcpy(response, ""); // no response

    printf("\nLOG:\n\n");

    if (count == 0)
    {
        printf("Log empty\n\n");
        return;
    }

    for (int i = 0; i < count; i++)
    {
        log_entry_t *e = log_get(i);
        log_format_entry(e, line, sizeof(line), false);

        printf("%s\n", line);
    }

    printf("\n");
}


/* ----------------------------------------------------------
   The functions below are called from above OR from router.c
   ---------------------------------------------------------- */

/* Adds a new user */
int exec_cmd_add(const char *phonenr, const char *who)
{
    int result = phonebook_add(phonenr);

    if (result == PB_OK)
    {
        modem_send_sms(phonenr, "Hallo, Welkom bij de sms rolluik bediening. stuur \"Op\" om het rolluik omhoog, en \"Neer\" om het rolluik omlaag te sturen.");
        log_add("ADD", phonenr, who, true);

        return PB_OK;
    }
    
    log_add("ADD", phonenr, who, false);

    return result;
}

/* Deletes a user */
int exec_cmd_del(const char *phonenr, const char *who)
{
    char number[PHONENR_SIZE];

    if (!phone_normalize(number, phonenr))
    {
        return ERR_PB_INVALID_NUMBER;
    }

    /* Als admin verwijderd wordt, check of laatste admin */
    if (phonebook_is_admin(number))
    {
        if (phonebook_count_admins() <= 1)
        {
            return ERR_PB_LAST_ADMIN;
        }
    }

    int result = phonebook_remove(number);

    log_add("DEL", number, who, result == PB_OK);
 
    return result;
}

/* promotes a user to an administrator */
int exec_cmd_promote(const char *phonenr, const char *who)
{
    char number[PHONENR_SIZE];
    char sender[PHONENR_SIZE];

    if (!phone_normalize(number, phonenr))
    {
        return ERR_PB_INVALID_NUMBER;
    }

    /* if user is already an admin */
    if(phonebook_is_admin(number)) {
        return ERR_PB_ALREADY_ADMIN;
    }

    int result = phonebook_set_admin(number, 1);

    log_add("PROMOTE", number, who, result == PB_OK);

    if(result == PB_OK) {
        modem_send_sms(number, "Gefeliciteerd, je bent nu een administrator!. sms HELP voor een overzicht van de functies.");
    }

    return result;
}

/* demotes an administrator to an normal user */
int exec_cmd_demote(const char *phonenr, const char *who)
{
    char number[PHONENR_SIZE];
    char sender[PHONENR_SIZE];

    if (!phone_normalize(number, phonenr))
    {
        return ERR_PB_INVALID_NUMBER;
    }

    phone_normalize(sender, who);

    /* SMS: do not demote yourself */
    if(strcmp(number, sender) == 0)
    {
        return ERR_PB_NOT_YOURSELF;
    }

    /* if user is already a normal user */
    if (!phonebook_is_admin(number))
    {
        return ERR_PB_NOT_ADMIN;
    }

    /* do not demote the last admin */
    if (phonebook_count_admins() <= 1)
    {
        return ERR_PB_LAST_ADMIN;
    }

    int result = phonebook_set_admin(number, 0);

    log_add("DEMOTE", number, who, result == PB_OK);

    return result;
}

/* swaps an user between admin and normal user */
int exec_cmd_swap(const char *phonenr, const char *who)
{
    char number[PHONENR_SIZE];
    char response[256];
    int  result;

    if (!phone_normalize(number, phonenr))
    {
        return ERR_PB_INVALID_NUMBER;
    }
    
    /* if user is an admin */
    if (phonebook_is_admin(number))
    {
        result = exec_cmd_demote(number, who);
    } else {
        result = exec_cmd_promote(number, who);
    }

    return result;
}

void exec_cmd_info(char lines[INFO_LINES][INFO_LINE_LEN])
{
    char uptime[32];
    char tempbuf[64];

    get_uptime_string(uptime);

    /* Regel 1 */
    strcpy(lines[0], "SMSDOOR v");
    strcat(lines[0], VERSION);

    /* Regel 2 */
    strcpy(lines[1], "Uptime: ");
    strcat(lines[1], uptime);

    /* Regel 3 */
    strcpy(lines[2], "System time: ");
    if (clock_get_time(tempbuf))
        strcat(lines[2], tempbuf);
    else
        strcat(lines[2], "NOT SET");

    /* Regel 4 */
    sprintf(lines[3],
        "Users %d Admins %d",
        phonebook_count(),
        phonebook_count_admins());

    /* Regel 5 */
    if (cfg.close_time == CLOSE_DISABLED)
    {
        strcpy(lines[4], "Auto close time: OFF");
    }
    else
    {
        int h = cfg.close_time / 60;
        int m = cfg.close_time % 60;

        sprintf(lines[4],
            "Auto close time: %d:%02d",
            h, m);
    }

    /* Regel 6 */
    strcpy(lines[5], "https://github.com/Fbeen/smsdoor");
}