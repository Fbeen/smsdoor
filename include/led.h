#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "hardware.h"

#define LED_COUNT 5

typedef struct
{
    uint8_t gpio;

    uint16_t interval;      // ms, 0 = always on
    uint16_t duration;      // ms, 0 = endless
    uint16_t counter;       // increased by ISR

    uint8_t state;
    uint8_t active;

} led_t;

void led_on(uint8_t gpio);
void led_off(uint8_t gpio);
void led_activate(uint8_t gpio, uint16_t interval, uint16_t duration);
void led_tick();

#endif