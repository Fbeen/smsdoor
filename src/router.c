#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "router.h"
#include "webserver.h"
#include "html/index_html_gz.h"
#include "pico/stdlib.h"
#include "commands.h"
#include "phonebook.h"
#include "modem.h"
#include "util.h"
#include "log.h"

/* ===== Helper ===== */

static void send_text(TCP_CONNECT_STATE_T *state,
                      struct tcp_pcb *pcb,
                      const char *text)
{
    state->file_data  = (const unsigned char*)text;
    state->result_len = strlen(text);

    ws_send_header(pcb, state, "text/plain", state->result_len, false);
    ws_send_chunk(pcb, state);
}

static int hex2int(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

/*
 * Haalt een query parameter uit een URI.
 * Voorbeeld:
 *   uri = "/add?nr=+31612345678&foo=bar"
 *   key = "nr"
 *   -> out = "+31612345678"
 *
 * Return:
 *   true  = gevonden
 *   false = niet gevonden
 */
bool get_query_param(const char *uri, const char *key, char *out, size_t out_size)
{
    const char *q = strchr(uri, '?');
    if (!q) return false;

    q++;
    size_t key_len = strlen(key);

    while (*q)
    {
        if (strncmp(q, key, key_len) == 0 && q[key_len] == '=')
        {
            const char *val = q + key_len + 1;
            size_t i = 0;

            while (*val && *val != '&' && i < out_size - 1)
            {
                if (*val == '%' && val[1] && val[2])
                {
                    int hi = (val[1] >= 'A') ? (val[1] & ~0x20) - 'A' + 10 : val[1] - '0';
                    int lo = (val[2] >= 'A') ? (val[2] & ~0x20) - 'A' + 10 : val[2] - '0';
                    out[i++] = (hi << 4) | lo;
                    val += 3;
                }
                else if (*val == '+')
                {
                    out[i++] = ' ';
                    val++;
                }
                else
                {
                    out[i++] = *val++;
                }
            }

            out[i] = '\0';
            return true;
        }

        while (*q && *q != '&') q++;
        if (*q == '&') q++;
    }

    return false;
}

/* ===== HANDLERS ===== */

static void handle_index(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    state->file_data  = index_html_gz;
    state->result_len = index_html_gz_len;

    ws_send_header(pcb, state, "text/html", state->result_len, true);
    ws_send_chunk(pcb, state);
}

static void handle_open(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    printf("Rolluik OPEN\n");
    send_text(state, pcb, "Rolluik gaat omhoog");
}

static void handle_close(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    printf("Rolluik CLOSE\n");
    send_text(state, pcb, "Rolluik gaat omlaag");
}

static void handle_overhead(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    printf("Overhead deur CLOSE\n");
    send_text(state, pcb, "Overheaddeur gaat dicht");
}

static void handle_info(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    // static char text[160];
    static char html[160] = "dit komt later.";

    // get_info(text);
    // nl2br(text, html, 160);

    state->file_data  = (const unsigned char*)html;
    state->result_len = strlen(html);

    ws_send_header(pcb, state, "application/json", state->result_len, false);
    ws_send_chunk(pcb, state);
}

/* ===== Future handlers ===== */

static void handle_users(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    phonebook_entry_t entry;
    int count = phonebook_count();
    #define RESPONSE_SIZE 512
    char response[RESPONSE_SIZE];

    int pos = 0;

    pos += snprintf(response + pos, RESPONSE_SIZE - pos, "{ \"users\": [");

    bool first = true;

    for (int i = 0; i < MAX_PHONES; i++)
    {
        if (phonebook_get(i, &entry))
        {
            if (!first)
            {
                pos += snprintf(response + pos, RESPONSE_SIZE - pos, ",");
            }

            pos += snprintf(
                response + pos,
                RESPONSE_SIZE - pos,
                "{ \"nr\": \"%s\", \"admin\": %s }",
                entry.number,
                entry.isAdmin ? "true" : "false"
            );

            first = false;
        }
    }

    pos += snprintf(response + pos, RESPONSE_SIZE - pos, "] }");

    state->file_data  = (const unsigned char*)response;
    state->result_len = strlen(response);

    ws_send_header(pcb, state, "application/json", state->result_len, false);
    ws_send_chunk(pcb, state);
}

static void handle_add_user(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char response[256];
    int pos = 0;
    char nr[32];
 
    // --- parameter ophalen ---
    get_query_param(state->uri, "nr", nr, sizeof(nr));

    printf(">>%s\n", nr);

    if (!nr || strlen(nr) == 0)
    {
        pos += snprintf(response + pos, sizeof(response) - pos, "{ \"err\": \"Missing number\" }");
    }
    else
    {
        int err = phonebook_add(nr);

        if (err == PB_OK)
        {
            // JSON response
            pos += snprintf(response + pos, sizeof(response) - pos, "{ \"err\": \"Ok\" }");

            // zelfde gedrag als console
            modem_send_sms(nr, "Hallo, Welkom bij de sms rolluik bediening. stuur \"Op\" om het rolluik omhoog, en \"Neer\" om het rolluik omlaag te sturen.");

            log_add("ADD", nr, "web", true);
        }
        else
        {
            pos += snprintf(response + pos, sizeof(response) - pos, "{ \"err\": \"%s\" }",phonebook_strerror(err));
            log_add("ADD", nr, "web", false);
        }
    }
    
    // --- response koppelen ---
    state->file_data  = (const unsigned char*)response;
    state->result_len = strlen(response);

    // --- header + verzenden ---
    ws_send_header(pcb, state, "application/json", state->result_len, false);
    ws_send_chunk(pcb, state);
}

static void handle_del_user(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    send_text(state, pcb, "Delete user");
}

static void handle_admin(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    send_text(state, pcb, "Admin page");
}

static void handle_log(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    send_text(state, pcb, "Log page");
}

static void handle_config(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    send_text(state, pcb, "Config page");
}

static void handle_reboot(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    send_text(state, pcb, "Rebooting...");
    // watchdog_reboot(0, 0, 0); later
}

/* ===== 404 ===== */

static void handle_notfound(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    send_text(state, pcb, "404 Not Found");
}

/* ===== ROUTE TABLE ===== */

static const route_t routes[] =
{
    { "/",           handle_index },
    { "/open",       handle_open },
    { "/close",      handle_close },
    { "/overhead",   handle_overhead },
    { "/info",       handle_info },
    { "/users",      handle_users },
    { "/add",        handle_add_user },
    { "/del",        handle_del_user },
    { "/admin",      handle_admin },
    { "/log",        handle_log },
    { "/config",     handle_config },
    { "/reboot",     handle_reboot },
};

#define ROUTE_COUNT (sizeof(routes) / sizeof(routes[0]))

/* ===== ROUTER ===== */

void router_handle_request(const char *path, TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    for (int i = 0; i < ROUTE_COUNT; i++)
    {
        if (strcmp(path, routes[i].path) == 0)
        {
            routes[i].handler(state, pcb);
            return;
        }
    }

    handle_notfound(state, pcb);
}