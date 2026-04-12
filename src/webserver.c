#include <string.h>
#include <stdlib.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "dhcpserver.h"
#include "dnsserver.h"
#include "webserver.h"
#include "router.h"

#define TCP_PORT 80
#define CHUNK_SIZE 1024

typedef struct {
    struct tcp_pcb *server_pcb;
    ip_addr_t gw;
} TCP_SERVER_T;

static TCP_SERVER_T server_state;

/*
Closes the TCP connection, unregisters callbacks,
and frees the client state memory.
*/
static err_t ws_close_connection(TCP_CONNECT_STATE_T *con_state, struct tcp_pcb *client_pcb, err_t close_err)
{
    if (client_pcb) {
        tcp_arg(client_pcb, NULL);
        tcp_sent(client_pcb, NULL);
        tcp_recv(client_pcb, NULL);
        tcp_err(client_pcb, NULL);

        err_t err = tcp_close(client_pcb);
        if (err != ERR_OK) {
            tcp_abort(client_pcb);
            close_err = ERR_ABRT;
        }
    }

    if (con_state)
        free(con_state);

    return close_err;
}

/*
Builds and sends the HTTP response header
(Content-Length, Content-Type, Connection).
*/
err_t ws_send_header(struct tcp_pcb *pcb, TCP_CONNECT_STATE_T *con_state, const char *content_type, uint32_t content_length, bool gzip)
{
    if (gzip)
    {
        con_state->header_len = snprintf(
            con_state->headers,
            sizeof(con_state->headers),
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %u\r\n"
            "Content-Type: %s\r\n"
            "Content-Encoding: gzip\r\n"
            "Connection: close\r\n"
            "\r\n",
            content_length,
            content_type
        );
    }
    else
    {
        con_state->header_len = snprintf(
            con_state->headers,
            sizeof(con_state->headers),
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %u\r\n"
            "Content-Type: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            content_length,
            content_type
        );
    }

    return tcp_write(pcb, con_state->headers, con_state->header_len, TCP_WRITE_FLAG_COPY);
}

/*
Sends the next chunk of the response body.
Used for chunked transmission of large files.
*/
err_t ws_send_chunk(struct tcp_pcb *pcb, TCP_CONNECT_STATE_T *con_state)
{
    int remaining = con_state->result_len - con_state->file_offset;
    if (remaining <= 0)
        return ERR_OK;

    int chunk_len = remaining;
    if (chunk_len > CHUNK_SIZE)
        chunk_len = CHUNK_SIZE;

    err_t err = tcp_write(pcb, con_state->file_data + con_state->file_offset, chunk_len, TCP_WRITE_FLAG_COPY);

    if (err == ERR_OK)
        con_state->file_offset += chunk_len;

    return err;
}

/*
Called when HTTP data is received from a client.
Parses the HTTP request, extracts the path,
calls the router, and starts sending the response.
*/
static err_t ws_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    TCP_CONNECT_STATE_T *state = (TCP_CONNECT_STATE_T*)arg;

    if (!p)
        return ws_close_connection(state, pcb, ERR_OK);

    if (state->request_handled)
    {
        tcp_recved(pcb, p->tot_len);
        pbuf_free(p);
        return ERR_OK;
    }

    int len = p->tot_len;
    if (len > (int)sizeof(state->headers) - 1)
        len = sizeof(state->headers) - 1;

    pbuf_copy_partial(p, state->headers, len, 0);
    state->headers[len] = '\0';

    /* Alleen eerste regel */
    char *line_end = strstr(state->headers, "\r\n");
    if (line_end) *line_end = '\0';

    printf("HTTP request: %s\n", state->headers);

    state->request_handled = 1;

    /* Parse: METHOD + URI */
    char method[8];
    char uri[128];

    if (sscanf(state->headers, "%7s %127s", method, uri) != 2)
    {
        pbuf_free(p);
        return ws_close_connection(state, pcb, ERR_OK);
    }

    /* Volledige URI opslaan */
    strncpy(state->uri, uri, sizeof(state->uri));
    state->uri[sizeof(state->uri) - 1] = '\0';

    /* PATH maken (zonder query) */
    strncpy(state->path, state->uri, sizeof(state->path));
    state->path[sizeof(state->path) - 1] = '\0';

    char *q = strchr(state->path, '?');
    if (q) *q = '\0';

    printf("PATH: %s\n", state->path);

    /* Router */
    router_handle_request(state->path, state, pcb);

    tcp_output(pcb);
    tcp_recved(pcb, p->tot_len);

    pbuf_free(p);
    return ERR_OK;
}

/*
Called when TCP data has been successfully sent.
Used to send the next chunk of the response.
Closes the connection when all data has been sent.
*/
static err_t ws_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    con_state->sent_len += len;

    /* Still body data left? Send next chunk */
    if (con_state->file_offset < con_state->result_len)
    {
        err_t err = ws_send_chunk(pcb, con_state);
        if (err != ERR_OK)
        {
            printf("tcp_send_next_chunk failed: %d\n", err);
            return ws_close_connection(con_state, pcb, err);
        }

        tcp_output(pcb);
        return ERR_OK;
    }

    /* Everything sent? Close connection */
    if (con_state->sent_len >= con_state->header_len + con_state->result_len)
        return ws_close_connection(con_state, pcb, ERR_OK);

    return ERR_OK;
}

/*
Called when a TCP connection error occurs.
Closes the connection and frees client resources.
*/
static void ws_err(void *arg, err_t err)
{
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    if (err != ERR_ABRT)
        ws_close_connection(con_state, con_state->pcb, err);
}

/*
Called when a new TCP client connects.
Allocates a client state structure and registers
recv, sent and error callbacks for this connection.
*/
static err_t ws_accept(void *arg, struct tcp_pcb *client_pcb, err_t err)
{
    if (err != ERR_OK || client_pcb == NULL)
        return ERR_VAL;

    TCP_CONNECT_STATE_T *con_state = calloc(1, sizeof(TCP_CONNECT_STATE_T));
    if (!con_state)
        return ERR_MEM;

    con_state->pcb = client_pcb;
    con_state->gw = &server_state.gw;

    tcp_arg(client_pcb, con_state);
    tcp_sent(client_pcb, ws_sent);
    tcp_recv(client_pcb, ws_recv);
    tcp_err(client_pcb, ws_err);

    return ERR_OK;
}

/*
Creates and opens the TCP listening socket on port 80
and registers the accept callback.
*/
static bool ws_open(const char *ap_name)
{
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb)
        return false;

    if (tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT))
        return false;

    server_state.server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!server_state.server_pcb)
        return false;

    tcp_arg(server_state.server_pcb, &server_state);
    tcp_accept(server_state.server_pcb, ws_accept);

    printf("Connect to WiFi '%s'\n", ap_name);
    return true;
}

/*
Initializes the webserver:
- Starts WiFi Access Point
- Starts DHCP server
- Starts DNS captive portal
- Opens TCP server on port 80
*/
void ws_init(void)
{
    const char *ap_name = "SMSDOOR";
    const char *password = "12345678";

    if (cyw43_arch_init()) {
        printf("WiFi init failed\n");
        return;
    }

    cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);

    ip4_addr_t mask;
    server_state.gw.addr = PP_HTONL(CYW43_DEFAULT_IP_AP_ADDRESS);
    mask.addr = PP_HTONL(CYW43_DEFAULT_IP_MASK);

    static dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &server_state.gw, &mask);

    static dns_server_t dns_server;
    dns_server_init(&dns_server, &server_state.gw);

    ws_open(ap_name);
}