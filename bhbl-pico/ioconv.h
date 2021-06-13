#pragma once

extern void ostr(char*p);
extern void onl();
extern void osp();
extern void ostrnl(char*p);
extern void o1hex(unsigned int u);
extern void o2hex(unsigned int u);
extern void o4hex(unsigned int u);
extern void o8hex(unsigned int u);
extern void o2hexdump(unsigned char*p,int n);
extern char*sdec(unsigned int u);
extern char*sfxp(unsigned int u,int q,int dp);
extern void odec(unsigned int u);
extern void osdec(int u);
extern void ofxp(unsigned int u,int q,int dp);
extern void osfxp(int u,int q,int dp);
extern char*sfloat(float f);

