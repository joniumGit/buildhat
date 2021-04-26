#include "hardware.h"
#include "hardware/gpio.h"

void leds_set(unsigned int u) {
  if(u&1) gpio_put(PIN_LED0,1); else gpio_put(PIN_LED0,0);
  if(u&2) gpio_put(PIN_LED1,1); else gpio_put(PIN_LED1,0);
  }

void init_leds() {
  leds_set(0);
  }
