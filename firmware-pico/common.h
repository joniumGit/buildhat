#pragma once

typedef unsigned char UC;
typedef unsigned int UI;

#define bootloader_pad ((volatile UC*)0x2003ff00)          // the last 256 bytes of main RAM are used to communication from bootloader to firmware

#define CLAMP(u,min,max) \
  if(u<(min)) u=(min); \
  if(u>(max)) u=(max);

#define ABS(x) (((x)<0)?-(x):(x))
