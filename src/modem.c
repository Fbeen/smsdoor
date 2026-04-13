#include <stdio.h>      // printf, putchar
#include <string.h>     // strstr, strcpy, strlen
#include <stdbool.h>    // bool, true, false
#include <stdlib.h>

#include "pico/stdlib.h"   // uart functies (RP2040)
#include "hardware/uart.h"
#include "hardware/watchdog.h"

#include "hardware.h"
#include "util.h"
#include "modem.h"
#include "commands.h"
#include "phonebook.h"
#include "clock.h"
#include "led.h"
#include "log.h"

line_buffer_t modem_line;

char sms_number[PHONENR_SIZE];
bool waiting_for_sms_text = false;
static char pin[SIM_PIN_SIZE] = "0000";

/* ----------------------------------------------------------
   Modem helpers
   ---------------------------------------------------------- */

void modem_send(const char *cmd)
{
    uart_puts(UART_MODEM, cmd);
    uart_puts(UART_MODEM, "\r\n");

    printf(">> %s\n", cmd);
}

bool modem_wait_for(const char *response, uint32_t timeout_ms)
{
    char buffer[256];
    int idx = 0;

    absolute_time_t timeout = make_timeout_time_ms(timeout_ms);

    while (!time_reached(timeout))
    {
        while (uart_is_readable(UART_MODEM))
        {
            char c = uart_getc(UART_MODEM);

            putchar(c); // debug output

            if (idx < sizeof(buffer) - 1)
            {
                buffer[idx++] = c;
                buffer[idx] = 0;



                if (strstr(buffer, response))
                    return true;
            }
        }
    }

    return false;
}

/* ----------------------------------------------------------
   Send SMS
   ---------------------------------------------------------- */

void modem_send_sms(const char *number, const char *text)
{
    char cmd[COMMAND_SIZE];

    sprintf(cmd, "AT+CMGS=\"%s\"", number);
    modem_send(cmd);

    if (modem_wait_for(">", 5000))
    {
        uart_puts(UART_MODEM, text);
        uart_putc(UART_MODEM, 0x1A); // CTRL+Z
        printf("\nSMS sent\n");
    }

    modem_wait_for("OK", 10000);
}

/* ----------------------------------------------------------
   Modem
   ---------------------------------------------------------- */

void modem_init()
{
    printf("\n--- Modem init ---\n");

    sleep_ms(6000); // modem boot time
    watchdog_update();

    // Test AT totdat modem reageert
    while (1)
    {
        modem_send("AT");
        if (modem_wait_for("OK", 2000))
            break;

        printf("No modem response, retry...\n");
        sleep_ms(2000);
        watchdog_update();
    }

    printf("\nModem responding\n");

    modem_send("ATE0");
    sleep_ms(200);

    modem_send("AT+CPIN?");
    sleep_ms(500);
    watchdog_update();
}

void modem_init_step2()
{
    // SMS storage SIM
    modem_send("AT+CPMS=\"SM\",\"SM\",\"SM\"");
    sleep_ms(200);

    // SMS text mode
    modem_send("AT+CMGF=1");
    sleep_ms(200);

    // SMS index notification
    modem_send("AT+CNMI=2,1,0,0,0");
    sleep_ms(200);

    watchdog_update();

    // Network time update
    modem_send("AT+CTZU=1");
    sleep_ms(200);

    modem_send("AT+CTZR=1");
    sleep_ms(200);

    // Ask network time
    modem_send("AT+CCLK?");

    watchdog_update();

    watchdog_update();

    printf("--- Modem init done ---\n\n");
    led_on(GPIO_LED_STATUS); 
}

void extract_sms_number(const char *line, char *number)
{
    const char *p = line;

    // eerste quoted veld overslaan
    const char *q1 = strchr(p, '"');
    if (!q1) return;
    const char *q2 = strchr(q1 + 1, '"');
    if (!q2) return;

    // tweede quoted veld (telefoonnummer)
    q1 = strchr(q2 + 1, '"');
    if (!q1) return;
    q2 = strchr(q1 + 1, '"');
    if (!q2) return;

    int len = q2 - q1 - 1;
    if (len > 0 && len < PHONENR_SIZE)
    {
        strncpy(number, q1 + 1, len);
        number[len] = 0;
    }
}

void modem_uart_task()
{
    command_t cmd;
    char response[MAX_CMD_LEN];
    static bool waiting_for_cmgr_text = false;
    static char sms_text[COMMAND_SIZE];
    static int sms_idx = 0;
    static int last_sms_index = 0;

    while (uart_is_readable(UART_MODEM))
    {
        char c = uart_getc(UART_MODEM);

        if (c == '\r' || c == '\n')
        {
            if (modem_line.index > 0)
            {
                modem_line.buffer[modem_line.index] = 0;

                printf("MODEM: %s\n", modem_line.buffer);

                /* ---------------- CMTI → SMS index ---------------- */
                if (strstr(modem_line.buffer, "+CMTI:"))
                {
                    int index = atoi(strrchr(modem_line.buffer, ',') + 1);
                    last_sms_index = index;

                    char cmd_buf[32];
                    sprintf(cmd_buf, "AT+CMGR=%d", index);
                    modem_send(cmd_buf);
                }

                /* ---------------- CMGR header ---------------- */
                else if (strstr(modem_line.buffer, "+CMGR:"))
                {
                    extract_sms_number(modem_line.buffer, sms_number);
                    waiting_for_cmgr_text = true;
                    sms_idx = 0;
                    sms_text[0] = 0;
                }

                /* ---------------- SMS tekst regels ---------------- */
                else if (waiting_for_cmgr_text)
                {
                    if (strcmp(modem_line.buffer, "OK") == 0)
                    {
                        // SMS compleet → command verwerken
                        cmd = make_command(sms_text, SRC_SMS, sms_number);
                        process_command(&cmd, response);

                        waiting_for_cmgr_text = false;

                        sleep_ms(1000);

                        char cmd_buf[32];
                        sprintf(cmd_buf, "AT+CMGD=%d", last_sms_index);
                        modem_send(cmd_buf);
                    }
                    else
                    {
                        // tekst regel toevoegen
                        int len = strlen(modem_line.buffer);

                        if (sms_idx + len + 2 < sizeof(sms_text))
                        {
                            strcpy(&sms_text[sms_idx], modem_line.buffer);
                            sms_idx += len;
                            sms_text[sms_idx++] = '\n';
                            sms_text[sms_idx] = 0;
                        }
                    }
                }

                /* ---------------- SIM status ---------------- */
                else if (strstr(modem_line.buffer, "CPIN:"))
                {
                    if (strstr(modem_line.buffer, "SIM PIN"))
                    {
                        printf("Unlocking SIM...\n");

                        char cmd_buf[32];
                        sprintf(cmd_buf, "AT+CPIN=\"%s\"", cfg.sim_pin);
                        modem_send(cmd_buf);
                    }
                    else if (strstr(modem_line.buffer, "READY"))
                    {
                        printf("SIM ready\n");

                        // NU PAS modem verder initialiseren
                        modem_init_step2();
                    }
                    else if (strstr(modem_line.buffer, "PUK"))
                    {
                        printf("SIM PUK required!\n");
                        led_activate(GPIO_LED_ERROR, 100, 0);
                        log_add("PUK ASKED", "", "system", false);
                    }
                }

                /* ---------------- CME ERROR ---------------- */
                else if (strstr(modem_line.buffer, "+CME ERROR"))
                {
                    printf("MODEM ERROR: %s\n", modem_line.buffer);

                    if (strstr(modem_line.buffer, "incorrect password"))
                    {
                        printf("Wrong SIM PIN!\n");
                        led_activate(GPIO_LED_ERROR, 200, 0);
                        log_add("PIN WRONG", "", "system", false);
                    }
                }

                /* ---------------- Network registratie ---------------- */
                else if (strstr(modem_line.buffer, "+CREG:") ||
                         strstr(modem_line.buffer, "+CGREG:") ||
                         strstr(modem_line.buffer, "+CEREG:"))
                {
                    if (strstr(modem_line.buffer, ",1") ||
                        strstr(modem_line.buffer, ",5"))
                    {
                        /* netwerk OK */
                    }
                    else
                    {
                        led_activate(GPIO_LED_ERROR, 500, 5000);
                    }
                }

                /* ---------------- Netwerktijd ---------------- */
                else if (strstr(modem_line.buffer, "+CTZV:") ||
                         strstr(modem_line.buffer, "+CCLK:"))
                {
                    /* set clock */
                    clock_set_clock(modem_line.buffer);
                }

                modem_line.index = 0;
            }
        }
        else
        {
            if (modem_line.index < LINE_BUFFER_SIZE - 1)
            {
                modem_line.buffer[modem_line.index++] = c;
            }
        }
    }
}
