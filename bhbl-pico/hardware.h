#pragma once

// control UART
#define UART_C uart0
#define UART_C_TXPIN 0
#define UART_C_RXPIN 1
#define UART_C_BAUD 115200 // 3000000

#define PIN_DEBUG0 2
#define PIN_DEBUG1 3

#define GPIO_READ(pin) (!!gpio_get(pin))
