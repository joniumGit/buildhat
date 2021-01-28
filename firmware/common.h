#pragma once

#include "stm32f413xx.h"

#define ARM_MATH_CM4 1

typedef unsigned char UC;
typedef unsigned int UI;

#define CLAMP(u,min,max) \
  if(u<(min)) u=(min); \
  if(u>(max)) u=(max);
