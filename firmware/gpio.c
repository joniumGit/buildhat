#include "gpio.h"

void init_gpio() {
  RCC->AHB1ENR|=0x0000001f;                        // enable clocks for GPIOA..E
  }

void gpio_out0(GPIO_TypeDef*port,int pin) {
  unsigned int u;
  port->BSRR=0x10000<<pin;                         // clear bit in ODR
  u=port->MODER;
  u&=~(3<<(pin*2));
  u|= (1<<(pin*2));                                // make an output
  port->MODER=u;
  port->PUPDR&=~(3<<(pin*2));                      // switch off pull-up and pull-down resistors
  }

void gpio_out1(GPIO_TypeDef*port,int pin) {
  unsigned int u;
  port->BSRR=1<<pin;                               // set bit in ODR
  u=port->MODER;
  u&=~(3<<(pin*2));
  u|= (1<<(pin*2));                                // make an output
  port->MODER=u;
  port->PUPDR&=~(3<<(pin*2));                      // switch off pull-up and pull-down resistors
  }

void gpio_inpu(GPIO_TypeDef*port,int pin) {
  int u;
  u=port->PUPDR;
  u&=~(3<<(pin*2));
  u|= (1<<(pin*2));                                // enable pull-up
  port->PUPDR=u;
  port->MODER&=~(3<<(pin*2));                      // make an input
  }

void gpio_in(GPIO_TypeDef*port,int pin) {
  port->PUPDR&=~(3<<(pin*2));                      // floating
  port->MODER&=~(3<<(pin*2));                      // make an input
  }

void gpio_ana(GPIO_TypeDef*port,int pin) {
  port->MODER|=3<<(pin*2);                         // make analogue
  }

void gpio_afr(GPIO_TypeDef*port,int pin,int afn) {
  unsigned int i,j,u;
  u=port->MODER;
  u&=~(3<<(pin*2));
  u|= (2<<(pin*2));                                // set to alternate function mode
  port->MODER=u;
  i=pin>>3;
  j=(pin&7)*4;
  u=port->AFR[i];
  u&=~(0xf<<j);
  u|=afn<<j;                                       // enter alternate function number
  port->AFR[i]=u;
  }
