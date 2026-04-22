#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "webserver.h"
#include "console.h"
#include "phonebook.h"
#include "hardware.h"
#include "commands.h"
#include "modem.h"
#include "util.h"
#include "clock.h"
#include "rshutter.h"
#include "led.h"

/* ----------------------------------------------------------
   TIMER
   ---------------------------------------------------------- */

static struct repeating_timer timer;

/* This function is called every tenth of a second (ISR context) */
bool timer_callback(struct repeating_timer *t)
{
    clock_tick();       /* see clock.c    */
    rshutter_tick();    /* see rshutter.c */
    led_tick();         /* see led.c      */

    return true;        /* repeat timer   */
}

/* ----------------------------------------------------------
   MAIN
   ---------------------------------------------------------- */

int main()
{
    /* enable watchdog, pico restarts if things go wrong */
    watchdog_enable(30000, 1);  // 15 seconds watchdog

    /* enable standard pico stuff */
    stdio_init_all();

    /* load config from flash*/
    config_init();

    /* sets the gpio's, see hardware.c */
    gpio_setup();

    /* Start repeating hardware timer */
    add_repeating_timer_ms(ISR_REPEAT_MS, timer_callback, NULL, &timer);

    /* flash status led during init */
    led_activate(GPIO_LED_STATUS, 500, 0); 

    /* setup uart to console and uart to modem */
    uart_setup();
    sleep_ms(2000);

    cprintf("\n[TC] Pico Rolluik Controller v%s starting...\n", VERSION);
    cprintf("[TC] Type \"HELP\" voor hulp!\n");

    /* loads the phonenumber whitelist from flash */
    phonebook_init();

    /* start webserver */
    ws_init();

    watchdog_update();

    /* init modem, see modem.c */
    modem_init();

    while (1)
    {
        /* tasks to do */
        console_uart_task();    /* see console.c  */
        modem_uart_task();      /* see modem.c    */
        clock_task();           /* see clock.c    */
        rshutter_task();        /* see rshutter.c */

        tight_loop_contents();
        watchdog_update();
    }
}