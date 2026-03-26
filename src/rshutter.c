#include <stdint.h>
#include <pico/stdlib.h>
#include "rshutter.h"
#include "hardware.h"
#include "config.h"

static uint16_t relay_remaining_seconds = 0;
static bool rshutter_must_stop = false;

void rshutter_up()
{
    rshutter_stop();
    gpio_put(GPIO_RELAY_UP, 1);
    gpio_put(GPIO_LED_UP, 1);
    relay_remaining_seconds = RELAY_TIMEOUT;
}

void rshutter_down()
{
    rshutter_stop();
    gpio_put(GPIO_RELAY_DOWN, 1);
    gpio_put(GPIO_LED_DOWN, 1);
    relay_remaining_seconds = RELAY_TIMEOUT;
}

void rshutter_stop()
{
    gpio_put(GPIO_RELAY_UP, 0);
    gpio_put(GPIO_LED_UP, 0);
    gpio_put(GPIO_RELAY_DOWN, 0);
    gpio_put(GPIO_LED_DOWN, 0);
    sleep_ms(500);
}

/* called 10 times per second by ISR */
void rshutter_tick()
{
    static uint8_t div = 0;
    div++;

    /* once per second */
    if(div >= 10)
    {
        div = 0;

        if(relay_remaining_seconds > 0)
        {
            relay_remaining_seconds--;

            if(relay_remaining_seconds == 0)
            {
                rshutter_must_stop = true;
            }
        }
    }
}

/* called from main loop */
void rshutter_task()
{
    if(rshutter_must_stop)
    {
        rshutter_must_stop = false;
        rshutter_stop();
    }
}
