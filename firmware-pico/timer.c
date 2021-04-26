#include "common.h"
#include "system.h"
#include "adc.h"
#include "leds.h"
#include "pico/stdlib.h"

static uint64_t time0;
static unsigned int tick;

//!!! void Timer7IRQ() {
//!!!   int u;
//!!!   TIM7->SR=0;
//!!!   tick++;
//!!!   if(tick&1) {
//!!!     adc_vref=ADC1->DR;                             // read VREFINT conversion result
//!!!     adc_trig(11);                                  // trigger Vin conversion
//!!!     }
//!!!   else {
//!!!     adc_vin=ADC1->DR;                              // read Vin conversion result
//!!!     adc_trig(17);                                  // trigger VREFINT conversion
//!!!     }
//!!!   if((tick&15)==15) {                              // every 16ms, and after the first few conversions have been done
//!!!     u=read_vin();
//!!!     if     (u<VIN_THRESH0) leds_set(0);
//!!!     else if(u<VIN_THRESH1) leds_set(3);            // orange
//!!!     else if(u<VIN_THRESH2) leds_set(2);            // green
//!!!     else                   leds_set(1);            // red
//!!!     }
//!!!   }

void init_timer() {
  time0=time_us_64();
  }

unsigned int gettick() {
  uint64_t t;
  t=time_us_64();
  while(t>time0) {
    time0+=1000;
    tick++;
    }
  return tick;
  }

// wait for a time period between n-1 and n ticks, n>0
void wait_ticks(unsigned int n) {
  n+=gettick();
  while(gettick()!=n) ;
  }

