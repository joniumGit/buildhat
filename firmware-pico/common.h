#pragma once

#define BLVERSION "BuildHAT bootloader version 1.0"

typedef unsigned char UC;
typedef unsigned int UI;

#define CLAMP(u,min,max) \
  if(u<(min)) u=(min); \
  if(u>(max)) u=(max);

#define ABS(x) (((x)<0)?-(x):(x))
