#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* version info */
#define VERSION "1.0"

/* Size of the buffer used to store incoming lines from the modem
Used for AT command responses, SMS messages, and status lines */
#define LINE_BUFFER_SIZE    128

/* Maximum length of a phone number string including '+' and null terminator
Example: "+31612345678" */
#define PHONENR_SIZE        16

/* Maximum number of phone numbers stored in the internal phonebook
These numbers are allowed to control the system */
#define MAX_PHONES          16

/* Maximum length of a received command string
Used for SMS commands or serial console commands like ADD, DEL, LIST */
#define COMMAND_SIZE        32

/* Timeout after which the relay is automatically turned off (seconds) */
#define RELAY_TIMEOUT       180; 


#define FLASH_TOTAL_SIZE       (2 * 1024 * 1024)

#define FLASH_CONFIG_OFFSET    (FLASH_TOTAL_SIZE - FLASH_SECTOR_SIZE)
#define FLASH_PHONEBOOK_OFFSET (FLASH_TOTAL_SIZE - 2 * FLASH_SECTOR_SIZE)


#define CONFIG_MAGIC   0x43464731   // "CFG1"
#define CONFIG_VERSION 2

#define SIM_PIN_SIZE 8

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t close_time; //  in minutes per day ( 0 .. 1439 )

    char sim_pin[SIM_PIN_SIZE];

} config_t;

extern config_t cfg;

#define CLOSE_DISABLED 0xFFFF

void config_init();
void config_save();
void config_set_pin(const char *pin);

#endif
