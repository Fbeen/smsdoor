#ifndef ROUTER_H
#define ROUTER_H

#include "lwip/tcp.h"

/* Forward declaration */
typedef struct TCP_CONNECT_STATE_T TCP_CONNECT_STATE_T;

/* Handler type */
typedef void (*route_handler_t)(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb);

/* Route entry */
typedef struct
{
    const char *path;
    route_handler_t handler;
} route_t;

void router_handle_request(const char *path, TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb);
void send_console_timeout(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb);
void send_console_json(TCP_CONNECT_STATE_T *state, struct tcp_pcb *pcb);


#endif