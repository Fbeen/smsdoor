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
#include "rshutter.h"

#define RESPONSE_SIZE 512

/* ===== Helper ===== */

static void send_text(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb, const char *text)
{
    state->file_data  = (const unsigned char*)text;
    state->result_len = strlen(text);

    ws_send_header(pcb, state, "text/plain", state->result_len, false);
    ws_send_chunk(pcb, state);
}

static void sendJson(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb, char *json)
{
    state->file_data  = (const unsigned char*)json;
    state->result_len = strlen(json);

    ws_send_header(pcb, state, "application/json", state->result_len, false);
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

void json_strip_braces(char *s)
{
    size_t len;

    if (s == NULL)
        return;

    len = strlen(s);

    /* minimaal "{}" */
    if (len < 2)
        return;

    /* moet beginnen met { en eindigen met } */
    if (s[0] != '{' || s[len - 1] != '}')
        return;

    /* alles 1 positie naar links schuiven */
    memmove(s, s + 1, len - 2);

    /* nieuwe afsluitende nul */
    s[len - 2] = '\0';
}

static void exec_command(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb, char *cmdtxt)
{
    command_t cmd;
    char buf[MAX_CMD_LEN];

    /* safe way of string copy */
    strncpy(buf, cmdtxt, MAX_CMD_LEN - 1);
    buf[MAX_CMD_LEN - 1] = '\0';

    cmd = make_command(buf, SRC_WEB, "web");
    process_command(&cmd, buf);

    send_text(state, pcb, buf);
}

static void get_users_json(char *response)
{
    phonebook_entry_t entry;
    int count = phonebook_count();
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
}

static void get_settings_json(char *response)
{
    sprintf(response, "{\"settings\":{\"ssid\":\"%s\",\"password\":\"%s\",\"simpin\":\"%s\",\"door\":%u,\"overhead\":%u}}", cfg.ssid, cfg.pass, cfg.sim_pin, cfg.duration_shutter, cfg.duration_overhead);
}

static void get_info_json(char *response)
{
    char tmpbuf[INFO_LINE_LEN];
    char lines[INFO_LINES][INFO_LINE_LEN];
    int i;

    /* vul array met regels */
    exec_cmd_info(lines);

    /* begin with json */
    strcpy(response, "{\"info\":[");

    /* alle regels samenvoegen met \n */
    for (i = 0; i < INFO_LINES; i++)
    {
        sprintf(tmpbuf, "\"%s\",", lines[i]);
        strcat(response, tmpbuf);
    }

    /* delete last comma */
    response[strlen(response)-1] = 0;

   /* end with json */
    strcat(response, "]}");
}

/* ===== HANDLERS ===== */

static void handle_index(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    state->file_data  = index_min_html_gz;
    state->result_len = index_min_html_gz_len;

    ws_send_header(pcb, state, "text/html", state->result_len, true);
    ws_send_chunk(pcb, state);
}

static void handle_open(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
     exec_command(state, pcb, "up");
}

static void handle_close(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    exec_command(state, pcb, "down");
}

static void handle_overhead(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    exec_command(state, pcb, "overhead down");
}

static void handle_info(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char response[RESPONSE_SIZE];

    get_info_json(response);

    sendJson(state, pcb, response);
}

/* ===== Future handlers ===== */

static void handle_users(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char response[RESPONSE_SIZE];

    get_users_json(response);

    sendJson(state, pcb, response);
}

static void handle_add_user(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char response[RESPONSE_SIZE];
    char nr[32];
 
    // --- parameter ophalen ---
    get_query_param(state->uri, "nr", nr, sizeof(nr));

    int err = exec_cmd_add(nr, "web");

    if (err == PB_OK)
    {
        sprintf(response, "{ \"err\": \"Ok\" }");
    }
    else
    {
        sprintf(response, "{ \"err\": \"%s\" }",phonebook_strerror(err));
    }
    
    sendJson(state, pcb, response);
}

static void handle_del_user(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char response[RESPONSE_SIZE];
    char nr[32];
 
    // --- parameter ophalen ---
    get_query_param(state->uri, "nr", nr, sizeof(nr));

    int err = exec_cmd_del(nr, "web");

    if (err == PB_OK)
    {
        sprintf(response, "{ \"err\": \"Ok\" }");
    }
    else
    {
        sprintf(response, "{ \"err\": \"%s\" }",phonebook_strerror(err));
    }
    
    sendJson(state, pcb, response);
}

static void handle_admin(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char response[RESPONSE_SIZE];
    char nr[32];
    int  result;

    // --- parameter ophalen ---
    get_query_param(state->uri, "nr", nr, sizeof(nr));

    result = exec_cmd_swap(nr, "web");

    if (result == PB_OK)
    {
        sprintf(response, "{ \"err\": \"Ok\" }");
    }
    else
    {
        sprintf(response, "{ \"err\": \"%s\" }",phonebook_strerror(result));
    }
    
    sendJson(state, pcb, response);
}

static void handle_status(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char users[256];
    char settings[256];
    char info[256];
    char response[RESPONSE_SIZE];

    get_users_json(users);
    json_strip_braces(users);

    get_settings_json(settings);
    json_strip_braces(settings);

    get_info_json(info);
    json_strip_braces(info);

    sprintf(response, "{%s,%s,%s}", users, settings, info);
    
    sendJson(state, pcb, response);
}

static void handle_settings(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char response[256];

    get_settings_json(response);
    
    sendJson(state, pcb, response);
}

static void handle_update_settings(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char response[256];
    char errors[5][48];
    char ssid[33];
    char pass[64];
    char spin[7];
    char temp[48];
    uint16_t door;
    uint16_t overhead;

    int  err_count = 0;

    get_query_param(state->uri, "ssid", ssid, sizeof(ssid));
    if (strlen(ssid) < 2)
    {
        strcpy(errors[err_count++], "SSID must be 2-32 characters long");
    }
    get_query_param(state->uri, "password", pass, sizeof(pass));
    if (strlen(pass) < 8)
    {
        strcpy(errors[err_count++], "PASSWORD must be 8-63 characters long");
    }
    get_query_param(state->uri, "simpin", spin, sizeof(spin));
    if (strlen(spin) < 4)
    {
        strcpy(errors[err_count++], "PIN must be 4-6 digits");
    } else {
        for (int i = 0; i < strlen(spin); i++)
        {
            if (!isdigit((unsigned char)spin[i]))
            {
                strcpy(errors[err_count++], "PIN must be numeric");
                break;
            }
        }
    }
    get_query_param(state->uri, "door", temp, sizeof(temp));
    door = (uint16_t)strtoul(temp, NULL, 10);
    if(door < 1 || door > 300) {
        strcpy(errors[err_count++], "runtime door must be 1..300 seconds");
    }
    get_query_param(state->uri, "overhead", temp, sizeof(temp));
    overhead = (uint16_t)strtoul(temp, NULL, 10);
    if(overhead < 1 || overhead > 300) {
        strcpy(errors[err_count++], "runtime overheaddoor must be 1..300 seconds");
    }

    if(err_count > 0) {
        // {"status":"error","errors":["runtime overheaddoor must be 1..300 seconds"]}
        strcpy(response, "{\"status\":\"error\",\"errors\":[");
        for(int i = 0 ; i < err_count ; i++) {
            sprintf(temp, "\"%s\",", errors[i]);
            strcat(response, temp);
        }
        response[strlen(response)-1] = 0; // delete last comma
        strcat(response, "]}");
    } else {
        config_set_pin(ssid);
        config_set_pass(pass);
        config_set_pin(spin);
        cfg.duration_shutter = door;
        cfg.duration_overhead = overhead;
        config_save();

        strcpy(response, "{\"status\":\"ok\"}");
    }

    sendJson(state, pcb, response);
}

/* ===== 404 ===== */

static void handle_notfound(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    // In captive portal mode: altijd index tonen
    handle_index(state, pcb);
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
    { "/adm",        handle_admin },
    { "/status",     handle_status },
    { "/settings",   handle_settings },
    { "/settings/update",   handle_update_settings },
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