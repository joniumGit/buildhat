#pragma once
#define NPORTS 4

// pins connected to LEDs: LED0 is red, LED1 is green
#define PIN_LED0       14
#define PIN_LED1       15
#define PIN_I2C0_SDA    8
#define PIN_I2C0_SCL    9
#define PIN_I2C1_SDA   18
#define PIN_I2C1_SCL   19


extern void init_ports();
