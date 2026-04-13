#include <stdlib.h>
#include <pico/stdlib.h>
#include "config.h"
#include "led.h"

led_t leds[LED_COUNT] =
{
    { GPIO_LED_UP, 0, 0, 0, 0 },
    { GPIO_LED_DOWN, 0, 0, 0, 0 },
    { GPIO_LED_STATUS, 0, 0, 0, 0 },
    { GPIO_LED_ERROR, 0, 0, 0, 0 },
};

/* lookup function */
static led_t *led_find(uint8_t gpio)
{
    for (int i = 0; i < LED_COUNT; i++)
    {
        if(gpio == leds[i].gpio)
            return &leds[i];
    }

    return NULL;
}

/* turn the led on */
void led_on(uint8_t gpio)
{
    led_activate(gpio, 0, 0);
}

/* turn the led off */
void led_off(uint8_t gpio)
{
    led_t *led = led_find(gpio);

    if(led == NULL)
        return;

    led->interval = 0;
    led->duration = 0;
    led->counter = 0;
    led->active = 0;
    led->state = 0;

    gpio_put(led->gpio, 0);
}

/* 
 * activates a led with a given interval and duration
 *
 * interval: for flashing set microseconds or zero for continuously
 * duration: the time before the LED is turned off or zero to leave the LED on
 */
void led_activate(uint8_t gpio, uint16_t interval, uint16_t duration)
{
    led_t *led = led_find(gpio);

    if(led == NULL)
        return;

    led->interval = interval / ISR_REPEAT_MS;
    led->duration = duration / ISR_REPEAT_MS;
    led->counter = 0;
    led->active = 1;
    led->state = 1;

    gpio_put(led->gpio, 1);
}

/* 
 * called 10 times per second by ISR
 *
 * switches on or off LEDs to let them flash
 * switches off LEDs when time is up
 */
void led_tick()
{
    for (int i = 0; i < LED_COUNT; i++)
    {
        if(leds[i].active)
        {
            leds[i].counter ++;

            if(leds[i].interval > 0 && leds[i].counter % leds[i].interval == 0)
            {
                if(leds[i].state) {
                    leds[i].state = 0;
                    gpio_put(leds[i].gpio, 0);
                } else {
                    leds[i].state = 1;
                    gpio_put(leds[i].gpio, 1);
                }
            }

            if(leds[i].duration > 0 && leds[i].counter >= leds[i].duration)
            {
                leds[i].interval = 0;
                leds[i].duration = 0;
                leds[i].active = 0;
                leds[i].state = 0;
                gpio_put(leds[i].gpio, 0);
            }
        }
    }
}