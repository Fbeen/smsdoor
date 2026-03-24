#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "phonebook.h"

#define PHONE_MIN_DIGITS 8
#define PHONE_MAX_DIGITS 15
#define DEFAULT_COUNTRY_CODE "31"

const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + FLASH_PHONEBOOK_OFFSET);

void phonebook_init(void)
{
    phonebook_t pb;

    memcpy(&pb, flash_target_contents, sizeof(phonebook_t));

    if (pb.magic != PHONEBOOK_MAGIC || pb.version != PHONEBOOK_VERSION)
    {
        printf("Phonebook flash invalid, initializing...\n");

        memset(&pb, 0, sizeof(phonebook_t));
        pb.magic = PHONEBOOK_MAGIC;
        pb.version = PHONEBOOK_VERSION;

        phonebook_save(&pb);
    }
    else
    {
        printf("Phonebook flash valid\n");
    }
}

void phonebook_load(phonebook_t *pb)
{
    memcpy(pb, flash_target_contents, sizeof(phonebook_t));
}

static void phonebook_save(phonebook_t *pb)
{
    uint32_t ints = save_and_disable_interrupts();

    flash_range_erase(FLASH_PHONEBOOK_OFFSET, 4096);
    flash_range_program(FLASH_PHONEBOOK_OFFSET, (uint8_t *)pb, sizeof(phonebook_t));

    restore_interrupts(ints);
}

int phonebook_add(const char *number)
{
    phonebook_t pb;
    char normalized[PHONENR_SIZE];

    if (!phone_normalize(normalized, number))
        return ERR_PB_INVALID_NUMBER;

    phonebook_load(&pb);

    /* Check of al bestaat */
    for (int i = 0; i < MAX_PHONES; i++)
    {
        if (strcmp(pb.entries[i].number, normalized) == 0)
            return ERR_PB_NUMBER_ALREADY_EXISTS;
    }

    /* Zoek lege plek */
    for (int i = 0; i < MAX_PHONES; i++)
    {
        if (pb.entries[i].number[0] == 0)
        {
            strncpy(pb.entries[i].number, normalized, PHONENR_SIZE - 1);
            pb.entries[i].number[PHONENR_SIZE - 1] = 0;

            /* Eerste nummer = admin */
            if (phonebook_count() == 0)
                pb.entries[i].isAdmin = 1;
            else
                pb.entries[i].isAdmin = 0;

            phonebook_save(&pb);
            return PB_OK;
        }
    }

    return ERR_PB_FULL;
}

int phonebook_remove(const char *number)
{
    phonebook_t pb;
    char normalized[PHONENR_SIZE];

    if (!phone_normalize(normalized, number))
        return ERR_PB_INVALID_NUMBER;

    phonebook_load(&pb);

    for (int i = 0; i < MAX_PHONES; i++)
    {
        if (strcmp(pb.entries[i].number, normalized) == 0)
        {
            pb.entries[i].number[0] = 0;
            pb.entries[i].isAdmin = 0;

            phonebook_save(&pb);
            return PB_OK;
        }
    }

    return ERR_PB_NUMBER_NOT_FOUND;
}

bool phonebook_exists(const char *number)
{
    phonebook_t pb;
    phonebook_load(&pb);

    for (int i = 0; i < MAX_PHONES; i++)
    {
        if (strcmp(pb.entries[i].number, number) == 0)
            return true;
    }

    return false;
}

int phonebook_count(void)
{
    phonebook_t pb;
    phonebook_load(&pb);

    int count = 0;

    for (int i = 0; i < MAX_PHONES; i++)
    {
        if (pb.entries[i].number[0])
            count++;
    }

    return count;
}

int phonebook_count_admins(void)
{
    phonebook_t pb;
    phonebook_load(&pb);

    int count = 0;

    for (int i = 0; i < MAX_PHONES; i++)
    {
        if (pb.entries[i].number[0] && pb.entries[i].isAdmin)
            count++;
    }

    return count;
}

bool phonebook_get(int index, phonebook_entry_t *entry)
{
    phonebook_t pb;
    phonebook_load(&pb);

    if (index < 0 || index >= MAX_PHONES)
        return false;

    if (pb.entries[index].number[0] == 0)
        return false;

    strcpy(entry->number, pb.entries[index].number);
    entry->isAdmin = pb.entries[index].isAdmin;
    
    return true;
}

bool phonebook_is_admin(const char *number)
{
    phonebook_t pb;
    phonebook_load(&pb);

    for (int i = 0; i < MAX_PHONES; i++)
    {
        if (strcmp(pb.entries[i].number, number) == 0)
            return pb.entries[i].isAdmin;
    }

    return false;
}

int phonebook_set_admin(const char *number, int isAdmin)
{
    phonebook_t pb;
    phonebook_load(&pb);

    for (int i = 0; i < MAX_PHONES; i++)
    {
        if (strcmp(pb.entries[i].number, number) == 0)
        {
            pb.entries[i].isAdmin = isAdmin ? 1 : 0;
            phonebook_save(&pb);
            return PB_OK;
        }
    }

    return ERR_PB_NUMBER_NOT_FOUND;
}

/*
 * Helper that makes numbers into the +CCNUMBER format
*/
int phone_normalize(char *dst, const char *src)
{
    char buf[32];
    int j = 0;

    if (!dst || !src)
        return 0;

    /* filter input */
    for (int i = 0; src[i] && j < (int)(sizeof(buf) - 1); i++)
    {
        if (isdigit((unsigned char)src[i]) || src[i] == '+')
        {
            buf[j++] = src[i];
        }
    }
    buf[j] = '\0';

    if (j == 0)
        return 0;

    /* '+' only at first position */
    for (int i = 1; buf[i]; i++)
    {
        if (buf[i] == '+')
            return 0;
    }

    /* convert */
    if (buf[0] == '+')
    {
        snprintf(dst, PHONENR_SIZE, "%s", buf);
    }
    else if (buf[0] == '0' && buf[1] == '0')
    {
        snprintf(dst, PHONENR_SIZE, "+%s", buf + 2);
    }
    else if (buf[0] == '0')
    {
        snprintf(dst, PHONENR_SIZE, "+%s%s", DEFAULT_COUNTRY_CODE, buf + 1);
    }
    else
    {
        return 0;
    }

    /* validate */
    int len = strlen(dst);
    int digits = len - 1;

    if (digits < PHONE_MIN_DIGITS || digits > PHONE_MAX_DIGITS)
        return 0;

    for (int i = 1; dst[i]; i++)
    {
        if (!isdigit((unsigned char)dst[i]))
            return 0;
    }

    return 1;
}

const char *phonebook_strerror(int err)
{
    switch (err)
    {
        case PB_OK:
            return "OK";
        case ERR_PB_INVALID_NUMBER:
            return "Invalid number";
        case ERR_PB_NUMBER_ALREADY_EXISTS:
            return "Number already exists";
        case ERR_PB_FULL:
            return "Phonebook full";
        case ERR_PB_SAVE_FAILED:
            return "Phonebook save failed";
        case ERR_PB_NUMBER_NOT_FOUND:
            return "PNumber not found";
        default:
            return "Unknown error";
    }
}