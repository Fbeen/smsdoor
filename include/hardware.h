#ifndef HARDWARE_H
#define HARDWARE_H

#define UART_DEBUG      uart0
#define UART_MODEM      uart1

#define UART_DEBUG_TX   0
#define UART_DEBUG_RX   1

#define UART_MODEM_TX   4
#define UART_MODEM_RX   5

#define UART_BAUDRATE   115200

#define GPIO_RELAY_UP   28
#define GPIO_RELAY_DOWN 28

#define GPIO_LED_UP     25
#define GPIO_LED_DOWN   25
#define GPIO_LED_STATUS 25
#define GPIO_LED_ERROR  25
#define GPIO_LED_PICO   25

#define GPIO_OVERHEAD   99

/* output pins */

void uart_setup();
void gpio_setup();

void roldeur_down_press();
void roldeur_down_release();

#endif