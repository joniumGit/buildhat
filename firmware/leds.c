#include "gpio.h"
#include "hardware.h"

void leds_set(unsigned int u) {
  if(u&1) gpio_out1(GPIO_LED0,PIN_LED0); else gpio_out0(GPIO_LED0,PIN_LED0);
  if(u&2) gpio_out1(GPIO_LED1,PIN_LED1); else gpio_out0(GPIO_LED1,PIN_LED1);
  }

void init_leds() {
  gpio_out0(GPIO_LED0,PIN_LED0);
  gpio_out0(GPIO_LED1,PIN_LED1);
  }
