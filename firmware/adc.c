#include "common.h"
#include "gpio.h"

static int vrefintcal;                             // calibrated VREFINT in V Q12
int adc_vref,adc_vin;                              // conversion results

void init_adc() {
  RCC->APB2ENR|=0x00000100;                        // enable clock for ADC
  ADC1->CR2=1;                                     // ADC on
  ADC1_COMMON->CCR=0x00830000;                     // 6.25MHz clock, VREFINT on
  ADC1->SMPR1=(3<<21)+(3<<3);                      // Tsa=56c on channels 11 and 17
  gpio_ana(GPIOC,1);
  vrefintcal=(*(short*)0x1fff7a2a)*33/10;          // VREFINT calibration value at 3.3V
  }

void adc_trig(int chan) {
  ADC1->SQR3=chan;
  ADC1->CR2|=1<<30;                                // start conversion
  }

// return measured input voltage in Q16 format
int read_vin() {
  int u;
  u=(((adc_vin<<16)/adc_vref)*vrefintcal)>>12;     // calibrate to V Q16
  return u*(47+10)/10;                             // correct for resistor divider on ADC input
  }

