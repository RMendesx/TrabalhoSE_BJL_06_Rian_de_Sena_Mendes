#ifndef LED_H
#define LED_H

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define LED_GREEN 11
#define LED_BLUE 12
#define LED_RED 13

void led_on(uint gpio) // Liga o LED
{
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, 1);
}

void led_off(uint gpio) // Desliga o LED
{
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, 0);
}

void led_azul()
{
    led_on(LED_BLUE);
    led_off(LED_GREEN);
    led_off(LED_RED);
}

void led_verde()
{
    led_on(LED_GREEN);
    led_off(LED_BLUE);
    led_off(LED_RED);
}

void led_vermelho()
{
    led_off(LED_GREEN);
    led_off(LED_BLUE);
    led_on(LED_RED);
}

void led_amarelo()
{
    led_on(LED_GREEN);
    led_off(LED_BLUE);
    led_on(LED_RED);
}

#endif