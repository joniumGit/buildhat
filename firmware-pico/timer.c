#include "common.h"
#include "hardware.h"
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
    adc_vin=adc_hw->result*57/10*3300/4096;              // in mV
    if     (adc_vin<VIN_THRESH0) leds_set(0);
    else if(adc_vin<VIN_THRESH1) leds_set(3);            // orange
    else if(adc_vin<VIN_THRESH2) leds_set(2);            // green
    else                         leds_set(1);            // red
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

