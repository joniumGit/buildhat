#pragma once

#include "common.h"
#include "hardware/i2c.h"

#define NPORTS 4

extern struct porthw {
  int pin_rts;                                       // pins used to drive the LPF2 interface circuitry
  int pin_dio;
  int pin_rx;
  int pin_tx;
  i2c_inst_t*i2c;                                    // port for communication with motor driver
  int i2c_add;
  } porthw[NPORTS];

// control UART
#define UART_C uart0
#define UART_C_TXPIN 0
#define UART_C_RXPIN 1
#define UART_C_BAUD 115200

// pins connected to LEDs: LED0 is red, LED1 is green
#define PIN_LED0       14
#define PIN_LED1       15
#define PIN_PORTFAULT  16                            // +3V3 to the ports overloaded; needs pullup
#define PIN_PORTON     17                            // enable +3V3 to the ports
#define PIN_MOTORFAULT 28                            // motor fault (wire-OR); needs pullup
#define PIN_ADCVIN     29                            // Vin*10/(10+47)

#define PIN_I2C0_SDA    8
#define PIN_I2C0_SCL    9
#define PIN_I2C1_SDA   18
#define PIN_I2C1_SCL   19

#define GPIO_READ(pin) (!!gpio_get(pin))
