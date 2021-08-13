#pragma once

#include "common.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/pio.h"

#define NPORTS 4

extern struct porthw {
  int pin_rts;                                             // pins used to drive the LPF2 interface circuitry
  int pin_dio;
  int pin_rx;
  int pin_tx;
  i2c_inst_t*i2c;                                          // port for communication with motor driver
  int i2c_add;
  PIO pio;
  int txsm;
  int rxsm;
  int irq;
  volatile io_rw_32*inte;
  unsigned int intb; // interrupt bits
  } porthw[NPORTS];

// INTE bit is intb&PORT_INTE_RXMASK or intb&PORT_INTE_TXMASK
#define PORT_INTE_RXMASK 0x0f
#define PORT_INTE_TXMASK 0xf0

// ACCELEROMETER
#define ACCEL_I2C i2c0
#define ACCEL_I2C_ADD 0x6a

// control UART
#define UART_C uart0
#define UART_C_TXPIN 0
#define UART_C_RXPIN 1
#define UART_C_BAUD 115200
// #define UART_C_BAUD 3000000                                // for better debugging, especially of PID loop

// pins connected to LEDs: LED0 is red, LED1 is green
#define PIN_LED0       14
#define PIN_LED1       15

#define PIN_PORTFAULT  16                                  // +3V3 to the ports overloaded; needs pullup
#define PIN_PORTON     17                                  // enable +3V3 to the ports
#define PIN_MOTORFAULT 28                                  // motor fault (wire-OR); needs pullup
#define PIN_ADCVIN     29                                  // Vin*10/(10+47)
#define ADC_CHAN        3

#define PIN_I2C0_SDA    8
#define PIN_I2C0_SCL    9
#define PIN_I2C1_SDA   18
#define PIN_I2C1_SCL   19

#define PIN_DEBUG0 2
#define PIN_DEBUG1 3

#define GPIO_READ(pin) (!!gpio_get(pin))

#define VIN_THRESH0 4800                                   // Vin threshold voltages for various LED colours in mV
#define VIN_THRESH1 6000
#define VIN_THRESH2 8500

extern void leds_set(int d);
