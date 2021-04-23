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
//  USART_TypeDef*uart;                              // corresponding UART
//  GPIO_TypeDef*port_d5; int pin_d5;
//  GPIO_TypeDef*port_d6; int pin_d6;
//  GPIO_TypeDef*port_in1;int pin_in1;               // PWM
//  GPIO_TypeDef*port_in2;int pin_in2;
  } porthw[NPORTS];

// control UART
#define UART_C uart0
#define UART_C_TXPIN 0
#define UART_C_RXPIN 1
#define UART_C_BAUD 115200

// pins connected to LEDs: LED0 is red, LED1 is green
#define PIN_LED0 14
#define PIN_LED1 15

#define GPIO_READ(pin) (!!gpio_get(pin))
