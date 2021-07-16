#pragma once

#define PWM_UPDATE 10                              // in milliseconds

#define WAVE_SQUARE 0
#define WAVE_SINE 1
#define WAVE_TRI 2
#define WAVE_PULSE 3

extern float pid_drive_limit;

extern void proc_pwm(int pn);
