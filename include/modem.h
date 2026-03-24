#ifndef MODEM_H
#define MODEM_H

void modem_send(const char *cmd);
bool modem_wait_for(const char *response, uint32_t timeout_ms);
void modem_send_sms(const char *number, const char *text);
void modem_init();
void extract_sms_number(const char *line, char *number);
void modem_uart_task();
bool modem_command(const char *cmd, char *response, int maxlen, uint32_t timeout_ms);
bool modem_get_time(char *datetime);

#endif