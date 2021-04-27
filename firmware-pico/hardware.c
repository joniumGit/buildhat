#include "hardware.h"

struct porthw porthw[NPORTS]={
//    rts dio  rx  tx  i2c add
    {  11, 10,  9,  8,i2c0,0x60+(1<<3)    },

//!!!    {  13, 12, 10, 11,i2c0,0x60+(0<<3)    },
//!!!    {   7,  6,  4,  5,i2c0,0x60+(1<<3)    },
//!!!    {  27, 26, 24, 25,i2c1,0x60+(2<<3)    },
//!!!    {  23, 22, 20, 21,i2c1,0x60+(3<<3)    }
  };

#define ACCEL_I2C i2c0
#define ACCEL_ADD 0x00 //!!!
