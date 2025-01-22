#pragma once

#define CTRLRXBLEN 8192                            // enough for 36ms of data at 3Mbaud
#define CTRLTXBLEN 8192

#define CMDBUFLEN 256                              // maximum length of a command

extern int echo;                                   // are we echoing characters back via the UART?

extern int i1chu();
extern void o1chu(int c);
extern int ctrl_ospace();
extern void init_control();
extern int w1ch();
extern void parse_reset();
extern int parse_eol();
extern void sksp();
extern void skspsc();
extern int parsehex(unsigned int*p);
extern int parseuint(unsigned int*p);
extern int parseint(int*p);
extern int parseq16(int*p);
extern int parsefloat(float*p);
extern int parsefmt(int*p);
extern int strmatch(char*s);
extern void proc_ctrl();
