#ifndef CONFIG_H
#define CONFIG_H

/* SIM card PIN code used to unlock the SIM after modem startup */
#define SIM_PIN             "4701"

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

#endif
