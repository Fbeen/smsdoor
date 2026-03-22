#include <stdio.h>
#include <string.h>
#include "util.h"
#include "phonebook.h"
#include "modem.h"
#include "commands.h"

command_t make_command(char *line, uint8_t source, const char *sender)
{
    command_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    str_trim(line);

    char *space = strchr(line, ' ');

    if (space)
    {
        *space = 0;

        strncpy(cmd.command, line, sizeof(cmd.command) - 1);
        strncpy(cmd.args, space + 1, sizeof(cmd.args) - 1);
    }
    else
    {
        strncpy(cmd.command, line, sizeof(cmd.command) - 1);
    }

    str_trim(cmd.command);
    str_to_upper(cmd.command);

    cmd.source = source;

    if (sender)
    {
        strncpy(cmd.sender, sender, sizeof(cmd.sender) - 1);
    }

    return cmd;
}

bool command_allowed(command_t *cmd)
{
    /* Console mag alles behalve INIT */
    if (cmd->source == SRC_CONSOLE)
    {
        if (strcmp(cmd->command, "INIT") == 0)
            return false;

        return true;
    }

    /* SMS zonder nummer → weigeren */
    if (cmd->sender[0] == 0)
        return false;

    /* INIT mag alleen als phonebook leeg */
    if (strcmp(cmd->command, "INIT") == 0)
    {
        if (phonebook_count() == 0)
            return true;
        else
            return false;
    }

    /* Phonebook leeg → verder niets toestaan */
    if (phonebook_count() == 0)
        return false;

    /* Phonebook gevuld → nummer moet bekend zijn */
    if (phonebook_exists(cmd->sender))
        return true;

    return false;
}

void process_command(command_t *cmd)
{
    int err;
    char response[160];
    sprintf(response, "%s:\n", cmd->command);

    /* refuse commands that are not allowed */
    if(!command_allowed(cmd))
    {
        if(cmd->source == SRC_CONSOLE)
            printf("Command %s from the console refused.\n\n", cmd->command);
        else
            printf("Command %s from number %s refused.\n\n", cmd->command, cmd->sender);

        return;
    }

    if (strcmp(cmd->command, "TEST") == 0)
    {
        strcat(response, "TEST command received!");
    }
    else if (strcmp(cmd->command, "INIT") == 0)
    {
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
    else if (strcmp(cmd->command, "ADD") == 0)
    {
        err = phonebook_add(cmd->args);
        if (err == PB_OK)
            strcat(response, "Number added");
        else
            strcat(response, phonebook_strerror(err));
    }
    else if (strcmp(cmd->command, "DEL") == 0)
    {
        char number[PHONENR_SIZE];
        char sender[PHONENR_SIZE];

        if (!phone_normalize(number, cmd->args))
        {
            strcat(response, "Invalid number");
        }
        else if (cmd->source == SRC_SMS)
        {
            phone_normalize(sender, cmd->sender);

            if (phonebook_count() <= 1)
            {
                strcat(response, "Cannot delete last number");
            }
            else if (strcmp(number, sender) == 0)
            {
                strcat(response, "Cannot delete own number");
            }
            else
            {
                int r = phonebook_remove(number);
                if(r == PB_OK)
                    strcat(response, "Number removed");
                else
                    strcat(response, phonebook_strerror(r));
            }
        }
        else
        {
            /* Console mag alles */
            int r = phonebook_remove(number);
            if(r == PB_OK)
                strcat(response, "Number removed");
            else
                strcat(response, phonebook_strerror(r));
        }
    }
    else if (strcmp(cmd->command, "LIST") == 0)
    {
        char number[PHONENR_SIZE];
        int count = phonebook_count();

        if (count == 0) {
            strcat(response, "Phonebook empty");
        } else {
            for (int i = 0; i < MAX_PHONES; i++)
            {
                if (phonebook_get(i, number))
                {
                    strcat(response, number);
                    strcat(response, "\n");
                }
            }
            sprintf(response, "%s\nTotal numbers: %d.", response, count);
        }
    }
    else
    {
        strcat(response, "Unknown command");
    }

    printf("%s\n\n", response);

    // Als SMS → stuur antwoord terug
    if (strlen(cmd->sender) > 0)
    {
        modem_send_sms(cmd->sender, response);
    }
}
