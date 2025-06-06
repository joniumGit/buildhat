// Copyright (c) 2025 Raspberry Pi (Trading) Ltd.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common.h"
#include "hardware.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"

static uint64_t time0;
static unsigned int tick;
unsigned int adc_vin;
int ledmode=-1;

// initialise ADC and tick timer
void init_timer() {
  adc_init();
  adc_gpio_init(PIN_ADCVIN);
  adc_select_input(ADC_CHAN); 
  adc_hw->cs|=4;                                           // trigger another conversion
  time0=time_us_64();
  }

unsigned int gettick() {
  uint64_t t;
  t=time_us_64();
  if(t>time0) {                                            // time for another update?
    adc_vin=adc_hw->result*57/10*3300/4096;                // in mV
    if(ledmode==-1) {                                      // set LEDs according to ledmode
      if     (adc_vin<VIN_THRESH0) leds_set(0);
      else if(adc_vin<VIN_THRESH1) leds_set(3);            // orange
      else if(adc_vin<VIN_THRESH2) leds_set(2);            // green
      else                         leds_set(1);            // red
    } else {
      leds_set(ledmode);
      }
    adc_hw->cs|=4;                                         // trigger another conversion
    while(t>time0) {
      time0+=1000;                                         // and read the result in at least 1ms' time
      tick++;                                              // count millisecond ticks
      }
    }
  return tick;
  }

// wait for a time period between n-1 and n ticks, n>0
void wait_ticks(unsigned int n) {
  n+=gettick();
  while(gettick()!=n) ;
  }
