#pragma once

#include "common.h"

#define NPORTS 4

extern struct porthw {
  GPIO_TypeDef*port_en; int pin_en;                // pins used to drive the LPF2 interface circuitry
  GPIO_TypeDef*port_rts;int pin_rts;
  GPIO_TypeDef*port_d5; int pin_d5;
  GPIO_TypeDef*port_d6; int pin_d6;
  GPIO_TypeDef*port_rx; int pin_rx;
  GPIO_TypeDef*port_tx; int pin_tx;
  USART_TypeDef*uart;                              // corresponding UART
  int uart_afn;                                    // its alternate function number
  GPIO_TypeDef*port_in1;int pin_in1;
  GPIO_TypeDef*port_in2;int pin_in2;
  TIM_TypeDef*timer;                               // corresponding timer
  int tim_afn;                                     // its alternate function number
  volatile uint32_t*tccr1,*tccr2;                  // capture/compare registers used
  } porthw[NPORTS];

// control UART
#define UART_C USART2
#define UART_C_IRQn USART2_IRQn
#define UART_C_RXPORT GPIOD
#define UART_C_RXPIN 6
#define UART_C_RXAFN 7
#define UART_C_TXPORT GPIOD
#define UART_C_TXPIN 5
#define UART_C_TXAFN 7

// sets of clocks that need to be enabled for the LPF ports
#define APB1CLOCKS_P RCC_APB1ENR_TIM4EN|RCC_APB1ENR_UART4EN|RCC_APB1ENR_UART5EN|RCC_APB1ENR_UART7EN|RCC_APB1ENR_UART8EN
#define APB2CLOCKS_P RCC_APB2ENR_TIM1EN

// clock that need to be enabled for the control port
#define APB1CLOCKS_C RCC_APB1ENR_USART2EN
#define APB2CLOCKS_C 0

// pins connected to LEDs: LED0 is red, LED1 is green
#define GPIO_LED0 GPIOA
#define GPIO_LED1 GPIOA
#define PIN_LED0  9
#define PIN_LED1 11

#define GPIO_READ(port,pin) ((((port)->IDR)>>(pin))&1)
