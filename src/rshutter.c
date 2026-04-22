#include <stdint.h>
#include <pico/stdlib.h>
#include "rshutter.h"
#include "hardware.h"
#include "config.h"
#include "led.h"

static uint16_t relay_remaining_seconds = 0;
static uint16_t overhead_remaining_seconds = 0;
static bool rshutter_must_stop = false;
static bool overhead_must_stop = false;

void rshutter_up()
{
    rshutter_stop();
    gpio_put(GPIO_RELAY_UP, 1);
    led_on(GPIO_LED_UP);
    relay_remaining_seconds = cfg.duration_shutter;
}

void rshutter_down()
{
    rshutter_stop();
    gpio_put(GPIO_RELAY_DOWN, 1);
    led_on(GPIO_LED_DOWN);
    relay_remaining_seconds = cfg.duration_shutter;
}

void overhead_down()
{
    led_activate(GPIO_LED_DOWN, 500, 60000);
    roldeur_down_press();
    overhead_remaining_seconds = cfg.duration_overhead;
}

void rshutter_stop()
{
    gpio_put(GPIO_RELAY_UP, 0);
    led_off(GPIO_LED_UP);
    gpio_put(GPIO_RELAY_DOWN, 0);
    led_off(GPIO_LED_DOWN);
    sleep_ms(500);
}

void overhead_stop()
{
    roldeur_down_release();
}

/* called 10 times per second by ISR */
void rshutter_tick()
{
    static uint16_t div = 0;
    div += ISR_REPEAT_MS;

    /* once per second */
    if(div >= 1000)
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

        if(overhead_remaining_seconds > 0)
        {
            overhead_remaining_seconds--;

            if(overhead_remaining_seconds == 0)
            {
                overhead_must_stop = true;
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
    if(overhead_must_stop)
    {
        overhead_must_stop = false;
        overhead_stop();
    }
}
