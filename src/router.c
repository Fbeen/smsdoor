#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "hardware/watchdog.h"
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
#include "console.h"
#include "tasks.h"

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
    uint8_t close_enabled = 1;
    uint8_t close_hour = 20;
    uint8_t close_min = 30;

    if (cfg.close_time == CLOSE_DISABLED)
    {
        close_enabled = 0;
    }
    else
    {
        close_hour = cfg.close_time / 60;
        close_min  = cfg.close_time % 60;
    }

    sprintf(
        response,
        "{\"settings\":{"
            "\"ssid\":\"%s\","
            "\"password\":\"%s\","
            "\"simpin\":\"%s\","
            "\"door\":%u,"
            "\"overhead\":%u,"
            "\"close_enabled\":%u,"
            "\"close_hour\":%u,"
            "\"close_min\":%u"
        "}}",
        cfg.ssid,
        cfg.pass,
        cfg.sim_pin,
        cfg.duration_shutter,
        cfg.duration_overhead,
        close_enabled,
        close_hour,
        close_min
    );
}

static void get_info_json(char *response)
{
    char tmpbuf[INFO_LINE_LEN];
    char line[INFO_LINE_LEN];
    int i;

    /* begin with json */
    strcpy(response, "{\"info\":[");

    /* alle regels samenvoegen met \n */
    for (i = 0; i < INFO_LINES; i++)
    {
        task_info_line(line, i);
        sprintf(tmpbuf, "\"%s\",", line);
        strcat(response, tmpbuf);
    }

    /* delete last comma */
    response[strlen(response)-1] = 0;

   /* end with json */
    strcat(response, "]}");
}

void get_console_json(char *json, uint32_t since)
{
    console_entry_t rows[LOG_LINES];
    int count;
    int pos = 0;
    uint32_t new_since = since;

    /* nieuwe regels ophalen */
    count = console_get_since(since, rows, LOG_LINES);

    /* begin JSON */
    pos += sprintf(json + pos,
        "{\"console\":{"
        "\"changed\":%s,"
        "\"since\":",
        (count > 0) ? "true" : "false"
    );

    /* laatste id bepalen */
    if (count > 0)
        new_since = rows[count - 1].id;

    pos += sprintf(json + pos, "%lu,", (unsigned long)new_since);

    /* logs array */
    pos += sprintf(json + pos, "\"logs\":[");

    for (int i = 0; i < count; i++)
    {
        pos += sprintf(json + pos, "\"");

        /* string escapen */
        const char *p = rows[i].text;
        while (*p)
        {
            char c = *p++;

            if (c == '"' || c == '\\')
            {
                json[pos++] = '\\';
                json[pos++] = c;
            }
            else if (c == '\n' || c == '\r')
            {
                /* overslaan */
            }
            else
            {
                json[pos++] = c;
            }
        }

        pos += sprintf(json + pos, "\"");

        if (i < count - 1)
            pos += sprintf(json + pos, ",");
    }

    /* einde */
    pos += sprintf(json + pos, "]}}");

    json[pos] = '\0';
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
    task_rshutter_up("wifi");
    send_text(state, pcb, "Rolluik gaat omhoog");
}

static void handle_close(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    task_rshutter_down("wifi");
    send_text(state, pcb, "Rolluik gaat omlaag");
}

static void handle_overhead(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    task_overhead_down("wifi");
    send_text(state, pcb, "Overheaddeur gaat dicht");
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

    int err = task_add_user(nr, "wifi");

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

    int err = task_delete_user(nr, "wifi");

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

    result = task_swap_admin(nr, "wifi");

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
    char logs[LOG_LINES * LOG_LEN / 2];
    char response[RESPONSE_SIZE * 2];

    get_users_json(users);
    json_strip_braces(users);

    get_settings_json(settings);
    json_strip_braces(settings);

    get_info_json(info);
    json_strip_braces(info);

    get_console_json(logs, 0);
    json_strip_braces(logs);

    sprintf(response, "{%s,%s,%s,%s}", users, settings, info, logs);
    
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
    char response[512];
    char errors[8][64];

    char ssid[33];
    char pass[64];
    char spin[7];
    char temp[48];

    uint16_t door;
    uint16_t overhead;

    uint8_t  close_enabled;
    uint8_t  close_hour;
    uint8_t  close_min;

    int err_count = 0;

    bool reset = false;

    /* ================= SSID ================= */
    get_query_param(state->uri, "ssid", ssid, sizeof(ssid));

    if (strlen(ssid) < 2 || strlen(ssid) > 32)
        strcpy(errors[err_count++], "SSID must be 2-32 characters long");

    /* ================= PASSWORD ================= */
    get_query_param(state->uri, "password", pass, sizeof(pass));

    if (strlen(pass) < 8 || strlen(pass) > 63)
        strcpy(errors[err_count++], "PASSWORD must be 8-63 characters long");

    /* ================= SIM PIN ================= */
    get_query_param(state->uri, "simpin", spin, sizeof(spin));

    if (strlen(spin) < 4 || strlen(spin) > 6)
    {
        strcpy(errors[err_count++], "PIN must be 4-6 digits");
    }
    else
    {
        for (int i = 0; i < strlen(spin); i++)
        {
            if (!isdigit((unsigned char)spin[i]))
            {
                strcpy(errors[err_count++], "PIN must be numeric");
                break;
            }
        }
    }

    /* ================= DOOR ================= */
    get_query_param(state->uri, "door", temp, sizeof(temp));
    door = (uint16_t)strtoul(temp, NULL, 10);

    if (door < 1 || door > 300)
        strcpy(errors[err_count++], "runtime door must be 1..300 seconds");

    /* ================= OVERHEAD ================= */
    get_query_param(state->uri, "overhead", temp, sizeof(temp));
    overhead = (uint16_t)strtoul(temp, NULL, 10);

    if (overhead < 1 || overhead > 300)
        strcpy(errors[err_count++], "runtime overheaddoor must be 1..300 seconds");

    /* ================= CLOSE ENABLED ================= */
    get_query_param(state->uri, "close_enabled", temp, sizeof(temp));
    close_enabled = (uint8_t)strtoul(temp, NULL, 10);

    /* ================= CLOSE HOUR ================= */
    get_query_param(state->uri, "close_hour", temp, sizeof(temp));
    close_hour = (uint8_t)strtoul(temp, NULL, 10);

    if (close_hour > 23)
        strcpy(errors[err_count++], "close hour must be 0..23");

    /* ================= CLOSE MIN ================= */
    get_query_param(state->uri, "close_min", temp, sizeof(temp));
    close_min = (uint8_t)strtoul(temp, NULL, 10);

    if (close_min > 59)
        strcpy(errors[err_count++], "close minute must be 0..59");

    /* ================= ERRORS ================= */
    if (err_count > 0)
    {
        strcpy(response, "{\"status\":\"error\",\"errors\":[");

        for (int i = 0; i < err_count; i++)
        {
            sprintf(temp, "\"%s\",", errors[i]);
            strcat(response, temp);
        }

        response[strlen(response) - 1] = 0; /* laatste komma weg */
        strcat(response, "]}");
    }
    else
    {
        if(strcmp(ssid, cfg.ssid) != 0) reset = true;
        if(strcmp(pass, cfg.pass) != 0) reset = true;
        if(strcmp(spin, cfg.sim_pin) != 0) reset = true;

        config_set_ssid(ssid);
        config_set_pass(pass);
        config_set_pin(spin);

        cfg.duration_shutter  = door;
        cfg.duration_overhead = overhead;

        if (close_enabled)
            cfg.close_time = (close_hour * 60) + close_min;
        else
            cfg.close_time = CLOSE_DISABLED;

        config_save();

        if(reset)
            strcpy(response, "{\"status\":\"reset\"}");
        else
            strcpy(response, "{\"status\":\"ok\"}");
    }

    sendJson(state, pcb, response);
}

static void handle_reset(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    watchdog_reboot(0, 0, 0); // reset pico
}

void send_console_timeout(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char json[] = "{\"console\":{\"changed\":false,\"since\":0,\"logs\":[]}}";
    sendJson(state, pcb, json);
}

void send_console_json(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char json[LOG_LINES * LOG_LEN / 2];

    get_console_json(json, state->since);
    sendJson(state, pcb, json);
}

static void handle_console(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char temp[6];

    get_query_param(state->uri, "since", temp, sizeof(temp));
    state->since = (uint16_t)strtoul(temp, NULL, 10);

    if (console_last_id() > state->since)
    {
        send_console_json(state, pcb);
        return;
    }

    /* nog niets veranderd -> long poll starten */
    state->longpoll_active = 1;
    state->poll_start_ms = to_ms_since_boot(get_absolute_time());
}

static void handle_cmd(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char response[RESPONSE_SIZE];
    char cmd_text[128];
    char cmdline[128];

    command_t cmd;

    /* parameter ophalen: /console/cmd?c=omlaag */
    get_query_param(state->uri, "c", cmd_text, sizeof(cmd_text));

    /* leeg commando? */
    if (cmd_text[0] == '\0')
    {
        sprintf(response, "{\"status\":\"error\",\"msg\":\"missing command\"}");
        sendJson(state, pcb, response);
        return;
    }

    /* voor de zekerheid kopie maken */
    strncpy(cmdline, cmd_text, sizeof(cmdline) - 1);
    cmdline[sizeof(cmdline) - 1] = '\0';

    /* zelfde route als UART console */
    cmd = make_command(cmdline, SRC_WIFI, "wifi");
    process_command(&cmd);

    /* process_command schrijft meestal zelf via cprintf/logs.
       JSON reply alleen status teruggeven */
    sprintf(response, "{\"status\":\"ok\"}");

    sendJson(state, pcb, response);
}
static void handle_ping(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char response[RESPONSE_SIZE];

    sprintf(response, "{\"status\":\"ok\"}");
 
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
    { "/console",    handle_console },
    { "/console/cmd",handle_cmd },
    { "/ping",       handle_ping },
    { "/reset/now",  handle_reset },
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