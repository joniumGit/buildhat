// Copyright (c) 2025 Raspberry Pi (Trading) Ltd.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

extern unsigned int adc_vin;   // ADC conversion result in mV
extern int ledmode;            // see "ledmode" command
#define VIN_THRESH0 4800       // Vin threshold voltages for various LED colours in mV
#define VIN_THRESH1 6000
#define VIN_THRESH2 8500

extern void init_timer();
extern unsigned int gettick();
extern void wait_ticks(unsigned int n);
