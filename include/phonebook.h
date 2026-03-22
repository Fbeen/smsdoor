#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define PHONEBOOK_MAGIC   0x50424B31   // "PBK1"
#define PHONEBOOK_VERSION 1

/* error codes phonebook_add function */
#define PB_OK                          0
#define ERR_PB_INVALID_NUMBER         -1
#define ERR_PB_NUMBER_ALREADY_EXISTS  -2
#define ERR_PB_FULL                   -3
#define ERR_PB_SAVE_FAILED            -4
#define ERR_PB_NUMBER_NOT_FOUND       -5


typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;

    char numbers[MAX_PHONES][PHONENR_SIZE];

} phonebook_t;

void phonebook_init(void);
void phonebook_load(phonebook_t *pb);
int  phonebook_add(const char *number);
int  phonebook_remove(const char *number);
bool phonebook_exists(const char *number);
int  phonebook_count(void);
bool phonebook_get(int index, char *number);
int  phone_normalize(char *dst, const char *src);
const char *phonebook_strerror(int err);

#endif