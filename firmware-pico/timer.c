#include "common.h"
#include "system.h"
#include "hardware.h"
#include "leds.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"

static uint64_t time0;
static unsigned int tick;
unsigned int adc_vin;

void init_timer() {
  adc_init();
  adc_gpio_init(PIN_ADCVIN);
  adc_select_input(ADC_CHAN); 
  adc_hw->cs|=4; // trigger another conversion
  time0=time_us_64();
  }

unsigned int gettick() {
  uint64_t t;
  t=time_us_64();
  if(t>time0) {
    adc_vin=adc_hw->result*57/10*3300/4096;  // in mV
    adc_hw->cs|=4; // trigger another conversion
    while(t>time0) {
      time0+=1000;
      tick++;
      }
    }
  return tick;
  }

// wait for a time period between n-1 and n ticks, n>0
void wait_ticks(unsigned int n) {
  n+=gettick();
  while(gettick()!=n) ;
  }

