#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "log.h"
#include "modem.h"
#include "phonebook.h"
#include "rshutter.h"
#include "tasks.h"
#include "util.h"

/* ----------------------------------------------------------
   The functions below are called from commands.c OR from router.c
   ---------------------------------------------------------- */

/* Adds a new user */
int task_add_user(const char *phonenr, const char *who)
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
int task_delete_user(const char *phonenr, const char *who)
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
int task_promote_user(const char *phonenr, const char *who)
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
int task_demote_user(const char *phonenr, const char *who)
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
int task_swap_admin(const char *phonenr, const char *who)
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
        result = task_demote_user(number, who);
    } else {
        result = task_promote_user(number, who);
    }

    return result;
}

void task_info_line(char *line, uint8_t i)
{
    char uptime[32];
    char tempbuf[64];

    switch(i)
    {
        case 0:
            sprintf(line, "SMSDOOR v%s", VERSION);
            break;

        case 1:
            get_uptime_string(uptime);
            sprintf(line, "Uptime: %s", uptime);
            break;

        case 2:
            if(clock_get_time(tempbuf))
                sprintf(line, "System time: %s", tempbuf);
            else
                sprintf(line, "System time: NOT SET");
            break;

        case 3:
            sprintf(line, "Users %d Admins %d",
                phonebook_count(),
                phonebook_count_admins());
            break;

        case 4:
            if(cfg.close_time == CLOSE_DISABLED)
            {
                sprintf(line, "Auto close time: OFF");
            }
            else
            {
                int h = cfg.close_time / 60;
                int m = cfg.close_time % 60;
                sprintf(line, "Auto close time: %d:%02d", h, m);
            }
            break;

        case 5:
            strcpy(line, "https://github.com/Fbeen/smsdoor");
            break;

        default:
            line[0] = 0;
    }
}

void task_rshutter_up(char *sender)
{
    rshutter_up();
    log_add("OPEN", "", sender, true);
}

void task_rshutter_down(char *sender)
{
    rshutter_down();
    log_add("CLOSE", "", sender, true);
}

void task_overhead_down(char *sender)
{
    overhead_down();
    log_add("OVERHEAD", "", sender, true);
}
