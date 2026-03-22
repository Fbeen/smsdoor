#ifndef HARDWARE_H
#define HARDWARE_H

#define UART_DEBUG      uart0
#define UART_MODEM      uart1

#define UART_DEBUG_TX   0
#define UART_DEBUG_RX   1

#define UART_MODEM_TX   4
#define UART_MODEM_RX   5

#define UART_BAUDRATE   115200


/* output pins */

void uart_setup();
void gpio_setup();

#endif