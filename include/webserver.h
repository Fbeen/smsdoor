#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "lwip/tcp.h"
#include "lwip/ip_addr.h"

/* Connection state struct */
typedef struct TCP_CONNECT_STATE_T {
    struct tcp_pcb *pcb;
    int sent_len;
    int file_offset;
    int result_len;
    int header_len;
    int request_handled;
    const unsigned char *file_data;
    char headers[512];
    char uri[128];   // volledig: /add?nr=...
    char path[64];   // alleen pad: /add
    ip_addr_t *gw;
    
    /* long polling */
    uint8_t longpoll_active;
    uint16_t since;
    uint32_t poll_start_ms;

} TCP_CONNECT_STATE_T;

/* Functions used by router */
err_t ws_send_header(struct tcp_pcb *pcb, TCP_CONNECT_STATE_T *con_state, const char *content_type, uint32_t content_length, bool gzip);
err_t ws_send_chunk(struct tcp_pcb *pcb, TCP_CONNECT_STATE_T *con_state);
void ws_stack_init(void);
void wifi_on_15min(void);
void wifi_off(void);
void wifi_task(void);
void wifi_button_task(void);


#endif