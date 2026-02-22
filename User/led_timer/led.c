
#include "gd32f4xx.h"

#define LED_GPIO_PORT  GPIOB
#define LED_PIN  GPIO_PIN_5

void led_init()
{
    /* ±÷”*/
    rcu_periph_clock_enable(RCU_GPIOA);
	gpio_mode_set(LED_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, LED_PIN);
	gpio_output_options_set(LED_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LED_PIN);
    gpio_bit_reset(LED_GPIO_PORT, LED_PIN);
}

void led_on()
{
    gpio_bit_set(LED_GPIO_PORT, LED_PIN);
}

void led_off()
{
    gpio_bit_reset(LED_GPIO_PORT, LED_PIN);
}
