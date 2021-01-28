#pragma once

extern int adc_vref,adc_vin;
extern void init_adc();
extern void adc_trig(int chan);
extern int read_vin();

#define MVTOVQ16(x) (((x)<<16)/1000)               // convert mV to V Q16
#define VIN_THRESH0 MVTOVQ16(4800)                 // Vin threshold voltages for various LED colours in mV
#define VIN_THRESH1 MVTOVQ16(6000)
#define VIN_THRESH2 MVTOVQ16(8500)

