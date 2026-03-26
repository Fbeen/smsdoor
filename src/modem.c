#include <stdio.h>      // printf, putchar
#include <string.h>     // strstr, strcpy, strlen
#include <stdbool.h>    // bool, true, false

#include "pico/stdlib.h"   // uart functies (RP2040)
#include "hardware/uart.h"

#include "hardware.h"
#include "util.h"
#include "modem.h"
#include "commands.h"
#include "phonebook.h"

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

    sleep_ms(5000); // modem boot time

    // Test AT totdat modem reageert
    while (1)
    {
        modem_send("AT");
        if (modem_wait_for("OK", 2000))
            break;

        printf("No modem response, retry...\n");
        sleep_ms(2000);
    }

    printf("\nModem responding\n");

    // Echo off
    modem_send("ATE0");
    modem_wait_for("OK", 2000);

    // SIM check
    modem_send("AT+CPIN?");
    if (modem_wait_for("SIM PIN", 3000))
    {
        printf("Unlocking SIM...\n");

        char cmd[32];
        sprintf(cmd, "AT+CPIN=\"%s\"", cfg.sim_pin);
        modem_send(cmd);
        modem_wait_for("OK", 5000);

        sleep_ms(3000);
    }

    // SMS text mode
    modem_send("AT+CMGF=1");
    modem_wait_for("OK", 2000);

    // Direct SMS ontvangen
    modem_send("AT+CNMI=2,2,0,0,0");
    modem_wait_for("OK", 2000);

    // Network time update
    modem_send("AT+CTZU=1");
    modem_wait_for("OK", 2000);

    modem_send("AT+CTZR=1");
    modem_wait_for("OK", 2000);

    printf("--- Modem init done ---\n\n");
}

void extract_sms_number(const char *line, char *number)
{
    const char *start = strchr(line, '\"');
    if (!start) return;

    const char *end = strchr(start + 1, '\"');
    if (!end) return;

    int len = end - start - 1;
    if (len > 0 && len < 31)
    {
        strncpy(number, start + 1, len);
        number[len] = 0;
    }
}

void modem_uart_task()
{
    command_t cmd;

    while (uart_is_readable(UART_MODEM))
    {
        char c = uart_getc(UART_MODEM);

        if (c == '\r' || c == '\n')
        {
            if (modem_line.index > 0)
            {
                modem_line.buffer[modem_line.index] = 0;

                printf("MODEM: %s\n", modem_line.buffer);

                /* +CMT header → nummer ophalen */
                if (strstr(modem_line.buffer, "+CMT:"))
                {
                    extract_sms_number(modem_line.buffer, sms_number);
                    waiting_for_sms_text = true;
                }
                else if (waiting_for_sms_text)
                {
                    /* Dit is de SMS tekst (command) */
                    cmd = make_command(modem_line.buffer, SRC_SMS, sms_number);
                    process_command(&cmd);

                    waiting_for_sms_text = false;
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

bool modem_command(const char *cmd, char *response, int maxlen, uint32_t timeout_ms)
{
    char line[128];
    int idx = 0;
    int resp_idx = 0;

    // UART buffer leegmaken
    while (uart_is_readable(UART_MODEM))
        uart_getc(UART_MODEM);

    // Command sturen
    uart_puts(UART_MODEM, cmd);
    uart_puts(UART_MODEM, "\r\n");

    absolute_time_t timeout = make_timeout_time_ms(timeout_ms);

    while (!time_reached(timeout))
    {
        while (uart_is_readable(UART_MODEM))
        {
            char c = uart_getc(UART_MODEM);

            // Debug output mag hier eventueel
            // putchar(c);

            if (c == '\r')
                continue;

            if (c == '\n')
            {
                line[idx] = 0;

                if (idx > 0)
                {
                    // Regel opslaan in response buffer
                    if (resp_idx + idx + 2 < maxlen)
                    {
                        strcpy(&response[resp_idx], line);
                        resp_idx += idx;
                        response[resp_idx++] = '\n';
                        response[resp_idx] = 0;
                    }

                    if (strcmp(line, "OK") == 0)
                        return true;

                    if (strstr(line, "ERROR"))
                        return false;
                }

                idx = 0;
            }
            else
            {
                if (idx < sizeof(line) - 1)
                    line[idx++] = c;
            }
        }
    }

    return false;
}
