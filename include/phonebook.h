#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define PHONEBOOK_MAGIC   0x50424B31   // "PBK1"
#define PHONEBOOK_VERSION 3

/* error codes phonebook_add function */
#define PB_OK                          0
#define ERR_PB_INVALID_NUMBER         -1
#define ERR_PB_NUMBER_ALREADY_EXISTS  -2
#define ERR_PB_FULL                   -3
#define ERR_PB_SAVE_FAILED            -4
#define ERR_PB_NUMBER_NOT_FOUND       -5
#define ERR_PB_LAST_ADMIN             -6

typedef struct
{
    char number[PHONENR_SIZE];
    uint8_t isAdmin;
    uint8_t reserved[3];   // padding / future use
} phonebook_entry_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;

    phonebook_entry_t entries[MAX_PHONES];

} phonebook_t;

void phonebook_init(void);
void phonebook_load(phonebook_t *pb);
static void phonebook_save(phonebook_t *pb);
int  phonebook_add(const char *number);
int  phonebook_remove(const char *number);
bool phonebook_exists(const char *number);
int  phonebook_count(void);
int  phonebook_count_admins(void);
bool phonebook_get(int index, phonebook_entry_t *entry);
int  phone_normalize(char *dst, const char *src);
bool phonebook_is_admin(const char *number);
int  phonebook_set_admin(const char *number, int isAdmin);
const char *phonebook_strerror(int err);

#endif