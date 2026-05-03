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
#include "console.h"

line_buffer_t modem_line;

char sms_number[PHONENR_SIZE];
static char modem_last_line[LINE_BUFFER_SIZE];
bool waiting_for_sms_text = false;
volatile bool modem_wait_flag = false;
static char modem_wait_match[32];
static bool sms_processed = false;

/* ----------------------------------------------------------
   Modem helpers
   ---------------------------------------------------------- */

void modem_send(const char *cmd)
{
    uart_puts(UART_MODEM, cmd);
    uart_puts(UART_MODEM, "\r\n");

    cprintf("[TM] %s\n", cmd);
}

static bool modem_last_line_is_error(void)
{
    if (strstr(modem_last_line, "ERROR"))
        return true;

    if (strstr(modem_last_line, "+CMS ERROR"))
        return true;

    if (strstr(modem_last_line, "+CME ERROR"))
        return true;

    return false;
}

void modem_feed_char(char c)
{
    static bool waiting_for_cmgr_text = false;
    static char sms_text[COMMAND_SIZE];
    static int sms_idx = 0;
    static int last_sms_index = 0;
    static bool modem_initialized = false;

    command_t cmd;

    /* ready to send sms body text */
    if (modem_wait_match[0] == '>' && c == '>')
    {
        modem_wait_flag = true;
        return;
    }

    if (c == '\r' || c == '\n')
    {
        if (modem_line.index == 0)
            return;

        modem_line.buffer[modem_line.index] = 0;

        strncpy(modem_last_line, modem_line.buffer, sizeof(modem_last_line) - 1);
        modem_last_line[sizeof(modem_last_line) - 1] = 0;

        // filter rommel
        if (strlen(modem_line.buffer) == 0)
        {
            modem_line.index = 0;
            return;
        }

        // logging
        if(strcmp(modem_line.buffer, "OK") != 0 && strcmp(modem_line.buffer, "") != 0) {
            cprintf("[FM] %s\n", modem_line.buffer);
        }

        // WAIT MATCH (belangrijk!)
        if (modem_wait_match[0] &&
            strstr(modem_line.buffer, modem_wait_match))
        {
            modem_wait_flag = true;
        }

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
            sms_processed = false;
            sms_idx = 0;
            sms_text[0] = 0;
        }

        /* ---------------- SMS tekst regels ---------------- */
        else if (waiting_for_cmgr_text)
        {
            if (strcmp(modem_line.buffer, "OK") == 0)
            {
                char sms_copy[COMMAND_SIZE];
                char number_copy[PHONENR_SIZE];
                int sms_index_copy;

                /* eerst de CMGR-state definitief afsluiten */
                waiting_for_cmgr_text = false;

                strncpy(sms_copy, sms_text, sizeof(sms_copy) - 1);
                sms_copy[sizeof(sms_copy) - 1] = 0;

                strncpy(number_copy, sms_number, sizeof(number_copy) - 1);
                number_copy[sizeof(number_copy) - 1] = 0;

                sms_index_copy = last_sms_index;

                /* optioneel: buffer resetten */
                sms_idx = 0;
                sms_text[0] = 0;

                /* nu pas command uitvoeren */
                cmd = make_command(sms_copy, SRC_SMS, number_copy);
                process_command(&cmd);

                sleep_ms(1000);

                char cmd_buf[32];
                sprintf(cmd_buf, "AT+CMGD=%d", sms_index_copy);
                modem_send(cmd_buf);
            }
            else
            {
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
                cprintf("[TC] Unlocking SIM...\n");

                char cmd_buf[32];
                sprintf(cmd_buf, "AT+CPIN=\"%s\"", cfg.sim_pin);
                modem_send(cmd_buf);
            }
            else if (strstr(modem_line.buffer, "READY"))
            {
                if (!modem_initialized)
                {
                    cprintf("[TC] SIM ready\n");
                    modem_initialized = true;
                    modem_init_step2();
                }
            }
            else if (strstr(modem_line.buffer, "PUK"))
            {
                cprintf("[TC] SIM PUK required!\n");
                led_activate(GPIO_LED_ERROR, 100, 0);
                log_add("PUK ASKED", "", "system", false);
            }
        }

        /* ---------------- CME ERROR ---------------- */
        else if (strstr(modem_line.buffer, "+CME ERROR"))
        {
            cprintf("[TC] MODEM ERROR: %s\n", modem_line.buffer);

            if (strstr(modem_line.buffer, "incorrect password"))
            {
                cprintf("[TC] Wrong SIM PIN!\n");
                led_activate(GPIO_LED_ERROR, 200, 0);
                log_add("PIN WRONG", "", "system", false);
            }
        }

        /* ---------------- Network registratie ---------------- */
        else if (strstr(modem_line.buffer, "+CREG:") ||
                 strstr(modem_line.buffer, "+CGREG:") ||
                 strstr(modem_line.buffer, "+CEREG:"))
        {
            if (!(strstr(modem_line.buffer, ",1") ||
                  strstr(modem_line.buffer, ",5")))
            {
                led_activate(GPIO_LED_ERROR, 500, 5000);
            }
        }

        /* ---------------- Netwerktijd ---------------- */
        else if (strstr(modem_line.buffer, "+CTZV:") ||
                 strstr(modem_line.buffer, "+CCLK:"))
        {
            clock_set_clock(modem_line.buffer);
        }

        modem_line.index = 0;
    }
    else
    {
        if (modem_line.index < LINE_BUFFER_SIZE - 1)
            modem_line.buffer[modem_line.index++] = c;
    }
}

bool modem_wait_for(const char *response, uint32_t timeout_ms)
{
    strncpy(modem_wait_match, response, sizeof(modem_wait_match) - 1);
    modem_wait_match[sizeof(modem_wait_match) - 1] = 0;

    modem_wait_flag = false;

    absolute_time_t timeout = make_timeout_time_ms(timeout_ms);

    while (!time_reached(timeout))
    {
        // BELANGRIJK: UART blijven verwerken
        while (uart_is_readable(UART_MODEM))
        {
            char c = uart_getc(UART_MODEM);
            modem_feed_char(c);
        }

        if (modem_wait_flag)
        {
            modem_wait_match[0] = 0;
            return true;
        }
    }

    modem_wait_match[0] = 0;
    return false;
}

/* ----------------------------------------------------------
   Send SMS
   ---------------------------------------------------------- */

void modem_send_sms(const char *number, const char *text)
{
    char cmd[COMMAND_SIZE];

    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", number);
    modem_send(cmd);

    /* wacht op prompt */
    if (!modem_wait_for(">", 5000))
    {
        cprintf("[TC] SMS failed: no prompt\n");
        return;
    }

    /* stuur tekst */
    uart_puts(UART_MODEM, text);
    uart_putc(UART_MODEM, 0x1A); // CTRL+Z

    /* wacht op resultaat */
    absolute_time_t timeout = make_timeout_time_ms(10000);

    while (!time_reached(timeout))
    {
        while (uart_is_readable(UART_MODEM))
        {
            char c = uart_getc(UART_MODEM);
            modem_feed_char(c);
        }

        if (strstr(modem_last_line, "OK"))
        {
            cprintf("[TC] SMS sent OK\n");
            return;
        }

        if (modem_last_line_is_error())
        {
            cprintf("[TC] SMS failed: %s\n", modem_last_line);
            return;
        }
    }

    cprintf("[TC] SMS timeout\n");
}

/* ----------------------------------------------------------
   Modem
   ---------------------------------------------------------- */

void modem_init()
{
    cprintf("[TC] Modem init\n");

    sleep_ms(6000); // modem boot time
    watchdog_update();

    // Test AT totdat modem reageert
    while (1)
    {
        modem_send("AT");
        if (modem_wait_for("OK", 2000))
            break;

        cprintf("[TC] No modem response, retry...\n");
        sleep_ms(2000);
        watchdog_update();
    }

    cprintf("[TC] Modem responding\n");

    modem_send("ATE0");
    modem_wait_for("OK", 2000);

    modem_send("AT+CPIN?");
    modem_wait_for("CPIN", 2000);
    watchdog_update();
}

void modem_init_step2()
{
    // SMS storage SIM
    modem_send("AT+CPMS=\"SM\",\"SM\",\"SM\"");
    modem_wait_for("OK", 2000);

    // SMS text mode
    modem_send("AT+CMGF=1");
    modem_wait_for("OK", 2000);

    // SMS index notification
    modem_send("AT+CNMI=2,1,0,0,0");
    modem_wait_for("OK", 2000);

    watchdog_update();

    // Network time update
    modem_send("AT+CTZU=1");
    modem_wait_for("OK", 2000);

    modem_send("AT+CTZR=1");
    modem_wait_for("OK", 2000);

    // Ask network time
    modem_send("AT+CCLK?");

    watchdog_update();

    cprintf("[TC] Modem init done\n");
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
    while (uart_is_readable(UART_MODEM))
    {
        char c = uart_getc(UART_MODEM);
        modem_feed_char(c);
    }
}