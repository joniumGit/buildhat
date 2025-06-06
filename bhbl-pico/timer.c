// Copyright (c) 2025 Raspberry Pi (Trading) Ltd.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "hardware.h"
#include "pico/stdlib.h"

static uint64_t time0;
static unsigned int tick;

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
