#pragma once

extern volatile unsigned int tick;

extern void Timer7IRQ();
extern void init_timer();
extern void wait_ticks(unsigned int n);
