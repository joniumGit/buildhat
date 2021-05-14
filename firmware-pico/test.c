#include "common.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "test.h"
#include "hardware.h"
#include "control.h"
#include "ioconv.h"
#include "ports.h"

int main() {
  init_timer();
  init_control();
  init_ports();
  while (1) {
    proc_ctrl();
    sleep_ms(250);
    }
  }
