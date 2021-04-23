#include <stdio.h>
#include "control.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "test.h"
#include "ioconv.h"

const uint LED_PIN = 25;

int main() {
  init_control();
//   stdio_uart_init_full(uart0,3000000,PICO_DEFAULT_UART_TX_PIN,PICO_DEFAULT_UART_RX_PIN);
   gpio_init(LED_PIN);
   gpio_set_dir(LED_PIN, GPIO_OUT);
   while (1) {
     gpio_put(LED_PIN, 0);
     sleep_ms(250);
     gpio_put(LED_PIN, 1);
     o8hex(0x12345678);
//     uart_puts(uart0,"Hello World\n");
     sleep_ms(1000);
     }
  }
