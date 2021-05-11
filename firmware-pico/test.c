#include <stdio.h>
#include "control.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "test.h"
#include "ioconv.h"

int main() {
  init_control();
  init_ports();
  while (1) {
    proc_ctrl();
    sleep_ms(250);
    }
  }
