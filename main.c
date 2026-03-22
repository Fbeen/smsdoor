#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "console.h"
#include "phonebook.h"
#include "hardware.h"
#include "commands.h"
#include "modem.h"
#include "util.h"

static struct repeating_timer timer;

static bool ledstat = false;

// Deze functie wordt elke seconde aangeroepen (ISR context)
bool timer_callback(struct repeating_timer *t)
{
    if(!ledstat) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1); // LED aan
        ledstat = true;
    } else {
        gpio_put(PICO_DEFAULT_LED_PIN, 0); // LED uit
        ledstat = false;
    }

    return true; // true = timer blijft herhalen
}

/* ----------------------------------------------------------
   MAIN
   ---------------------------------------------------------- */

int main()
{
    stdio_init_all();

    gpio_setup();

    // Start repeating timer elke 1000 ms
    add_repeating_timer_ms(1000, timer_callback, NULL, &timer);

    uart_setup();

    sleep_ms(2000);

    printf("\nPico Rolluik Controller starting...\n");

    phonebook_init();

    modem_init();

    while (1)
    {
        debug_uart_task();
        modem_uart_task();

        tight_loop_contents();
    }
}