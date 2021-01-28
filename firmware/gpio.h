#pragma once

#include "common.h"

extern void init_gpio();
extern void gpio_out0(GPIO_TypeDef*port,int pin);
extern void gpio_out1(GPIO_TypeDef*port,int pin);
extern void gpio_inpu(GPIO_TypeDef*port,int pin);
extern void gpio_in(GPIO_TypeDef*port,int pin);
extern void gpio_ana(GPIO_TypeDef*port,int pin);
extern void gpio_afr(GPIO_TypeDef*port,int pin,int afn);

