#pragma once

// #define DEBUG_PINS                                 // use MCU_CTS and MCU_RTS as debug signals
// #define DEBUG_BAUD 3000000
// #define DEBUG_BAUD 115200

extern int debug;
#define DEB_SER if(debug&(1<<0))                   // serial port debug
#define DEB_CON if(debug&(1<<1))                   // connect/disconnect debug
#define DEB_SIG if(debug&(1<<2))                   // signature debug
#define DEB_DPY if(debug&(1<<3))                   // DATA payload debug
#define DEB_PID if(debug&(1<<4))                   // PID controller debug

extern void o1chs(int x);
extern void ostrs(char*s);

#define i1ch i1chu                                 // use USART 2 for debug
#define o1ch o1chu                                 // use USART 2 for debug
#define ostr ostru
// #define i1ch i1chs                              // use semi-hosting for debug
// #define o1ch o1chs                              // use semi-hosting for debug
// #define ostr ostrs
