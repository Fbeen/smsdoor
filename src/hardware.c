#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware.h"

/* ----------------------------------------------------------
   UART
   ---------------------------------------------------------- */

void uart_setup()
{
    // Debug UART
    uart_init(UART_DEBUG, UART_BAUDRATE);
    gpio_set_function(UART_DEBUG_TX, GPIO_FUNC_UART);
    gpio_set_function(UART_DEBUG_RX, GPIO_FUNC_UART);

    // Modem UART
    uart_init(UART_MODEM, UART_BAUDRATE);
    gpio_set_function(UART_MODEM_TX, GPIO_FUNC_UART);
    gpio_set_function(UART_MODEM_RX, GPIO_FUNC_UART);
}

void gpio_setup()
{
    gpio_init(GPIO_LED_PICO);
    gpio_set_dir(GPIO_LED_PICO, GPIO_OUT);

    gpio_init(GPIO_RELAY_UP);
    gpio_set_dir(GPIO_RELAY_UP, GPIO_OUT);

    gpio_init(GPIO_RELAY_DOWN);
    gpio_set_dir(GPIO_RELAY_DOWN, GPIO_OUT);

    gpio_init(GPIO_LED_UP);
    gpio_set_dir(GPIO_LED_UP, GPIO_OUT);

    gpio_init(GPIO_LED_DOWN);
    gpio_set_dir(GPIO_LED_DOWN, GPIO_OUT);

    /* overhead door */
    gpio_init(GPIO_OVERHEAD );
    gpio_disable_pulls(GPIO_OVERHEAD );  // important!
    gpio_set_dir(GPIO_OVERHEAD , GPIO_IN);   // high-Z
}

void roldeur_down_press()
{
    gpio_set_dir(GPIO_OVERHEAD , GPIO_OUT);
    gpio_put(GPIO_OVERHEAD , 0);   // press button
}

void roldeur_down_release()
{
    gpio_set_dir(GPIO_OVERHEAD , GPIO_IN);   // high-Z
}
