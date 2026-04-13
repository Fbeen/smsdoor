#ifndef HARDWARE_H
#define HARDWARE_H

#define UART_DEBUG      uart0
#define UART_MODEM      uart1

#define UART_DEBUG_TX   0
#define UART_DEBUG_RX   1

#define UART_MODEM_TX   20
#define UART_MODEM_RX   21

#define UART_BAUDRATE   115200

#define GPIO_RELAY_UP   10
#define GPIO_RELAY_DOWN 3

#define GPIO_LED_UP     15
#define GPIO_LED_DOWN   6
#define GPIO_LED_STATUS 18
#define GPIO_LED_ERROR  19
#define GPIO_LED_PICO   25

#define GPIO_OVERHEAD   99

/* output pins */

void uart_setup();
void gpio_setup();


void roldeur_down_press();
void roldeur_down_release();

#endif