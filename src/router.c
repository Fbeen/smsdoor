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

/* ===== Helper ===== */

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
    if (out_size > 0)
        out[0] = '\0';

    if (!uri || !key || !out || out_size == 0)
        return false;
    
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

void json_escape(char *dst, const char *src, size_t maxlen)
{
    size_t i = 0;

    while (*src && i < maxlen - 1)
    {
        char c = *src++;

        if (c == '"' || c == '\\')
        {
            if (i < maxlen - 2)
            {
                dst[i++] = '\\';
                dst[i++] = c;
            }
        }
        else if (c == '\n' || c == '\r')
        {
            // skip of vervangen door spatie
        }
        else
        {
            dst[i++] = c;
        }
    }

    dst[i] = '\0';
}

static int json_append(char *buf, int pos, int max, const char *fmt, ...)
{
    if (max <= 0) return 0;
    if (pos >= max - 1) {
        buf[max - 1] = '\0';
        return max - 1;
    }

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + pos, max - pos, fmt, args);
    va_end(args);

    if (n < 0)
        return pos;

    if (pos + n >= max)
        return max - 1;   // ruimte houden voor '\0'

    return pos + n;
}

static int get_users_json_inner(char *buf, int pos, int max)
{
    phonebook_entry_t entry;
    char number[PHONENR_SIZE];
    bool first = true;

    pos = json_append(buf, pos, max, "\"users\":[");

    for (int i = 0; i < MAX_PHONES; i++)
    {
        if (phonebook_get(i, &entry))
        {
            if (!first)
                pos = json_append(buf, pos, max, ",");

            json_escape(number, entry.number, sizeof(number));

            pos = json_append(
                buf, pos, max,
                "{\"nr\":\"%s\",\"admin\":%s}",
                number,
                entry.isAdmin ? "true" : "false"
            );

            first = false;
        }
    }

    pos = json_append(buf, pos, max, "]");
    return pos;
}

static int get_settings_json_inner(char *buf, int pos, int max)
{
    uint8_t close_enabled = 1;
    uint8_t close_hour = 20;
    uint8_t close_min = 30;
    char ssid[33];
    char pass[64];
    char spin[7];

    json_escape(ssid, cfg.ssid, sizeof(ssid));
    json_escape(pass, cfg.pass, sizeof(pass));
    json_escape(spin, cfg.sim_pin, sizeof(spin));

    if (cfg.close_time == CLOSE_DISABLED)
    {
        close_enabled = 0;
    }
    else
    {
        close_hour = cfg.close_time / 60;
        close_min  = cfg.close_time % 60;
    }

    pos = json_append(
        buf, pos, max,
        "\"settings\":{"
            "\"ssid\":\"%s\","
            "\"password\":\"%s\","
            "\"simpin\":\"%s\","
            "\"door\":%u,"
            "\"overhead\":%u,"
            "\"close_enabled\":%u,"
            "\"close_hour\":%u,"
            "\"close_min\":%u"
        "}",
        ssid,
        pass,
        spin,
        cfg.duration_shutter,
        cfg.duration_overhead,
        close_enabled,
        close_hour,
        close_min
    );

    return pos;
}

static int get_info_json_inner(char *buf, int pos, int max)
{
    char line[INFO_LINE_LEN];
    char esc[INFO_LINE_LEN];
    int i;
    bool first = true;

    /* begin with json */
    pos = json_append(buf, pos, max, "\"info\":[");

    /* alle regels samenvoegen met \n */
    for (i = 0; i < INFO_LINES; i++)
    {
        if (!first)
            pos = json_append(buf, pos, max, ",");

        task_info_line(line, i);
        json_escape(esc, line, sizeof(esc));
        pos = json_append(buf, pos, max, "\"%s\"", esc);

        first = false;
    }

    /* end with json */
    pos = json_append(buf, pos, max, "]");

    return pos;
}

static int get_console_json_inner(char *buf, int pos, int max, uint32_t since)
{
    static console_entry_t rows[LOG_LINES];
    int count;
    uint32_t new_since = since;
    bool first = true;

    char esc[LOG_LEN];

    /* nieuwe regels ophalen */
    count = console_get_since(since, rows, LOG_LINES);

    if (count > 0)
        new_since = rows[count - 1].id;

    /* header */
    pos = json_append(buf, pos, max,
        "\"console\":{"
        "\"changed\":%s,"
        "\"since\":%lu,"
        "\"logs\":[",
        (count > 0) ? "true" : "false",
        (unsigned long)new_since
    );

    /* logs */
    for (int i = 0; i < count; i++)
    {
        if (!first)
            pos = json_append(buf, pos, max, ",");

        json_escape(esc, rows[i].text, sizeof(esc));

        pos = json_append(buf, pos, max, "\"%s\"", esc);

        first = false;
    }

    /* afsluiten */
    pos = json_append(buf, pos, max, "]}");

    return pos;
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
    sendJson(state, pcb, "{\"status\": \"ok\",\"msg\": \"Rolluik gaat omhoog\"}");
}

static void handle_close(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    task_rshutter_down("wifi");
    sendJson(state, pcb, "{\"status\": \"ok\",\"msg\": \"Rolluik gaat omlaag\"}");
}

static void handle_overhead(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    task_overhead_down("wifi");
    sendJson(state, pcb, "{\"status\": \"ok\",\"msg\": \"Overheaddeur gaat dicht\"}");
}

static void handle_info(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char *response = state->response;
    int max = sizeof(state->response);
    int pos = 0;

    pos = json_append(response, pos, max, "{");
    pos = get_info_json_inner(response, pos, max);
    pos = json_append(response, pos, max, "}");

    response[pos] = '\0';

    sendJson(state, pcb, response);
}

static void handle_users(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char *response = state->response;
    int max = sizeof(state->response);
    int pos = 0;

    pos = json_append(response, pos, max, "{");
    pos = get_users_json_inner(response, pos, max);
    pos = json_append(response, pos, max, "}");

    response[pos] = '\0';

    sendJson(state, pcb, response);
}

static void handle_add_user(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char *response = state->response;
    int max = sizeof(state->response);
    char nr[32];
 
    // --- parameter ophalen ---
    get_query_param(state->uri, "nr", nr, sizeof(nr));

    int err = task_add_user(nr, "wifi");

    if (err == PB_OK)
    {
        snprintf(response, max, "{\"status\":\"ok\",\"msg\":\"Nummer toegevoegd\"}");
    }
    else
    {
        snprintf(response, max, "{\"status\":\"error\",\"errors\":[\"%s\"]}",phonebook_strerror(err));
    }
    
    sendJson(state, pcb, response);
}

static void handle_del_user(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char *response = state->response;
    int max = sizeof(state->response);
    char nr[32];
 
    // --- parameter ophalen ---
    get_query_param(state->uri, "nr", nr, sizeof(nr));

    int err = task_delete_user(nr, "wifi");

    if (err == PB_OK)
    {
        snprintf(response, max, "{\"status\": \"ok\",\"msg\": \"Nummer verwijderd\"}");
    }
    else
    {
        snprintf(response, max, "{\"status\":\"error\",\"errors\":[\"%s\"]}",phonebook_strerror(err));
    }
    
    sendJson(state, pcb, response);
}

static void handle_admin(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char *response = state->response;
    int max = sizeof(state->response);
    char nr[32];
    int  result;

    // --- parameter ophalen ---
    get_query_param(state->uri, "nr", nr, sizeof(nr));

    result = task_swap_admin(nr, "wifi");

    if (result == PB_OK)
    {
        snprintf(response, max, "{\"status\":\"ok\"}");
    }
    else
    {
        snprintf(response, max, "{\"status\":\"error\",\"errors\":[\"%s\"]}",phonebook_strerror(result));
    }
    
    sendJson(state, pcb, response);
}

static void handle_status(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char *response = state->response;
    int max = sizeof(state->response);
    int pos = 0;

    pos = json_append(response, pos, max, "{");

    pos = get_users_json_inner(response, pos, max);
    pos = json_append(response, pos, max, ",");

    pos = get_settings_json_inner(response, pos, max);
    pos = json_append(response, pos, max, ",");

    pos = get_info_json_inner(response, pos, max);
    pos = json_append(response, pos, max, ",");

    pos = get_console_json_inner(response, pos, max, 0);

    pos = json_append(response, pos, max, "}");

    response[pos] = '\0';

    sendJson(state, pcb, response);
}

static void handle_settings(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char *response = state->response;
    int max = sizeof(state->response);
    int pos = 0;

    pos = json_append(response, pos, max, "{");
    pos = get_settings_json_inner(response, pos, max);
    pos = json_append(response, pos, max, "}");

    response[pos] = '\0';
    
    sendJson(state, pcb, response);
}

static void handle_update_settings(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char *response = state->response;
    int max = sizeof(state->response);
    int pos = 0;

    char errors[8][64];

    char ssid[33];
    char pass[64];
    char spin[7];
    char temp[48];

    uint16_t door = 0;
    uint16_t overhead = 0;

    uint8_t  close_enabled = 0;
    uint8_t  close_hour = 0;
    uint8_t  close_min = 0;

    int err_count = 0;
    bool reset = false;

    /* ================= SSID ================= */
    get_query_param(state->uri, "ssid", ssid, sizeof(ssid));

    if (strlen(ssid) < 2 || strlen(ssid) > 32)
        if (err_count < 8)
            snprintf(errors[err_count++], sizeof(errors[0]),
                     "SSID must be 2-32 characters long");

    /* ================= PASSWORD ================= */
    get_query_param(state->uri, "password", pass, sizeof(pass));

    if (strlen(pass) < 8 || strlen(pass) > 63)
        if (err_count < 8)
            snprintf(errors[err_count++], sizeof(errors[0]),
                     "PASSWORD must be 8-63 characters long");

    /* ================= SIM PIN ================= */
    get_query_param(state->uri, "simpin", spin, sizeof(spin));

    if (strlen(spin) < 4 || strlen(spin) > 6)
    {
        if (err_count < 8)
            snprintf(errors[err_count++], sizeof(errors[0]),
                     "PIN must be 4-6 digits");
    }
    else
    {
        for (int i = 0; i < strlen(spin); i++)
        {
            if (!isdigit((unsigned char)spin[i]))
            {
                if (err_count < 8)
                    snprintf(errors[err_count++], sizeof(errors[0]),
                             "PIN must be numeric");
                break;
            }
        }
    }

    /* ================= DOOR ================= */
    get_query_param(state->uri, "door", temp, sizeof(temp));
    door = (uint16_t)strtoul(temp, NULL, 10);

    if (door < 1 || door > 300)
        if (err_count < 8)
            snprintf(errors[err_count++], sizeof(errors[0]),
                     "runtime door must be 1..300 seconds");

    /* ================= OVERHEAD ================= */
    get_query_param(state->uri, "overhead", temp, sizeof(temp));
    overhead = (uint16_t)strtoul(temp, NULL, 10);

    if (overhead < 1 || overhead > 300)
        if (err_count < 8)
            snprintf(errors[err_count++], sizeof(errors[0]),
                     "runtime overheaddoor must be 1..300 seconds");

    /* ================= CLOSE ENABLED ================= */
    get_query_param(state->uri, "close_enabled", temp, sizeof(temp));
    close_enabled = (uint8_t)strtoul(temp, NULL, 10);

    /* ================= CLOSE HOUR ================= */
    get_query_param(state->uri, "close_hour", temp, sizeof(temp));
    close_hour = (uint8_t)strtoul(temp, NULL, 10);

    if (close_hour > 23)
        if (err_count < 8)
            snprintf(errors[err_count++], sizeof(errors[0]),
                     "close hour must be 0..23");

    /* ================= CLOSE MIN ================= */
    get_query_param(state->uri, "close_min", temp, sizeof(temp));
    close_min = (uint8_t)strtoul(temp, NULL, 10);

    if (close_min > 59)
        if (err_count < 8)
            snprintf(errors[err_count++], sizeof(errors[0]),
                     "close minute must be 0..59");

    /* ================= RESPONSE ================= */
    if (err_count > 0)
    {
        pos = json_append(response, pos, max,
                          "{\"status\":\"error\",\"errors\":[");

        for (int i = 0; i < err_count; i++)
        {
            if (i > 0)
                pos = json_append(response, pos, max, ",");

            pos = json_append(response, pos, max,
                              "\"%s\"", errors[i]);
        }

        pos = json_append(response, pos, max, "]}");
    }
    else
    {
        if (strcmp(ssid, cfg.ssid) != 0) reset = true;
        if (strcmp(pass, cfg.pass) != 0) reset = true;
        if (strcmp(spin, cfg.sim_pin) != 0) reset = true;

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

        if (reset)
            pos = json_append(response, pos, max, "{\"status\":\"reset\"}");
        else
            pos = json_append(response, pos, max, "{\"status\":\"ok\"}");
    }

    response[pos] = '\0';

    sendJson(state, pcb, response);
}

static void handle_reset(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    watchdog_reboot(0, 0, 0); // reset pico
}

void send_console_timeout(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    sendJson(state, pcb, "{\"console\":{\"changed\":false,\"since\":0,\"logs\":[]}}");
}

void send_console_json(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    char *response = state->response;
    int max = sizeof(state->response);
    int pos = 0;

    pos = json_append(response, pos, max, "{");
    pos = get_console_json_inner(response, pos, max, state->since);
    pos = json_append(response, pos, max, "}");

    response[pos] = '\0';

    sendJson(state, pcb, response);
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
    char cmd_text[128];
    char cmdline[128];

    command_t cmd;

    /* parameter ophalen: /console/cmd?c=omlaag */
    get_query_param(state->uri, "c", cmd_text, sizeof(cmd_text));

    /* leeg commando? */
    if (cmd_text[0] == '\0')
    {
        sendJson(state, pcb, "{\"status\":\"error\",\"errors\":[\"missing command\"]}");
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
    sendJson(state, pcb, "{\"status\":\"ok\"}");
}

static void handle_ping(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb)
{
    sendJson(state, pcb, "{\"status\":\"ok\"}");
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
    { "/users/add",  handle_add_user },
    { "/users/del",  handle_del_user },
    { "/users/adm",  handle_admin },
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