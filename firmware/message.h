#pragma once

extern void device_sendsys(int dn,unsigned char b);
extern void device_sendmessage(int dn,int b0,int b1,int logplen,unsigned char*payload);
extern void device_sendchar (int dn,int b0,int b1,unsigned char  v);
extern void device_sendshort(int dn,int b0,int b1,unsigned short v);
extern void device_sendint  (int dn,int b0,int b1,unsigned int   v);
extern void device_sendselect(int d,int m);
extern int proc_setupmessages(int pn);
extern int proc_datamessages(int pn);
