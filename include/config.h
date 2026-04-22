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

/* ISR time interval in milliseconds */
#define ISR_REPEAT_MS       100

#define FLASH_TOTAL_SIZE       (2 * 1024 * 1024)

#define FLASH_CONFIG_OFFSET    (FLASH_TOTAL_SIZE - FLASH_SECTOR_SIZE)
#define FLASH_PHONEBOOK_OFFSET (FLASH_TOTAL_SIZE - 2 * FLASH_SECTOR_SIZE)

#define WIFI_SSID_SIZE          33   // 32 + null
#define WIFI_PASS_SIZE          64   // 63 + null

#define CONFIG_MAGIC            0x43464731   // "CFG1"
#define CONFIG_VERSION          4

#define SIM_PIN_SIZE            8

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t close_time; //  in minutes per day ( 0 .. 1439 )

    char reserved;
    char ssid[WIFI_SSID_SIZE];
    char pass[WIFI_PASS_SIZE];

    char sim_pin[SIM_PIN_SIZE];

    uint16_t duration_shutter; // the duration that the relay of the roller shutter is switched on in seconds
    uint16_t duration_overhead; // the duration that the relay of the overheaddoor is switched on in seconds

} config_t;

extern config_t cfg;

#define CLOSE_DISABLED 0xFFFF

void config_init();
void config_save();
void config_set_pin(const char *pin);
void config_set_ssid(const char *ssid);
void config_set_pass(const char *pass);

#endif
