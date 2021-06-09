#pragma once

#define CTRLRXBLEN 4096                            // enough for 10ms of data at 3Mbaud
#define CTRLTXBLEN 4096

#define CMDBUFLEN 256

extern int i1chu();
extern void o1chu(int c);
extern void init_control();
extern int w1ch();
extern void parse_reset();
extern int parse_eol();
extern void sksp();
extern void skspsc();
extern int parseuint(unsigned int*p);
extern int parseint(int*p);
extern int parseq16(int*p);
extern int parsefloat(float*p);
extern int parsefmt(unsigned int*p);
extern int strmatch(char*s);
extern void proc_ctrl();
