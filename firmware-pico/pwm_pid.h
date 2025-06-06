// Copyright (c) 2025 Raspberry Pi (Trading) Ltd.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#define WAVE_SQUARE 0
#define WAVE_SINE 1
#define WAVE_TRI 2
#define WAVE_PULSE 3
#define WAVE_RAMP 4
#define WAVE_VAR 5 // special value for set point derived from a variable

extern void proc_pwm(int pn);
