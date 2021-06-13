#include <string.h>
#include "control.h"

void ostr(char*p) { while(*p) o1ch(*p++); }

void onl() {ostr("\r\n");}
void osp() {o1ch(' ');}
void ostrnl(char*p) { ostr(p); onl();}
void o1hex(unsigned int u) {u&=0x0f; if(u>=10) o1ch(u-10+'A'); else o1ch(u+'0');}
void o2hex(unsigned int u) {o1hex(u>> 4); o1hex(u);}
void o4hex(unsigned int u) {o2hex(u>> 8); o2hex(u);}
void o8hex(unsigned int u) {o4hex(u>>16); o4hex(u);}

void o2hexdump(unsigned char*p,int n) {
  while(n>1) { o2hex(*p++); osp(); n--; }
  if(n>0) o2hex(*p);
  }

// unsigned int to decimal string
char*sdec(unsigned int u) {
  static char b[12];
  char*p=b+12;
  *--p=0;
  do { *--p='0'+u%10; u/=10; } while(u);
  return p;
  }

// unsigned fixed point (Qq, q<=28) to decimal string, dp decimal places
char*sfxp(unsigned int u,int q,int dp) {
  int i;
  unsigned int r;
  static char b[24];
  if(dp>10) dp=10;
  r=1<<q;
  for(i=0;i<dp;i++) r/=10;
  u+=r/2;                                          // rounding
  strcpy(b,sdec(u>>q));
  if(dp>0) {
    i=strlen(b);
    b[i++]='.';
    u<<=28-q;
    while(dp>0) {
      u=((u<<4)>>4)*10;
      b[i++]='0'+(u>>28);
      dp--;
      }
    b[i]=0;
    }
  return b;
  }

void odec(unsigned int u) {
  char b[12],*p=b+12;
  *--p=0;
  do { *--p='0'+u%10; u/=10; } while(u);
  ostr(p);
  }

void osdec(int u) {
  if     (u<0) o1ch('-'),u=-u;
  else if(u>0) o1ch('+');
  else         osp();
  odec(u);
  }

void ofxp(unsigned int u,int q,int dp) { ostr(sfxp(u,q,dp)); }

void osfxp(int u,int q,int dp) {
  if(u<0) { o1ch('-'); u=-u; }
  ostr(sfxp(u,q,dp));
  }

char*sfloat(float f) {
  static char b[24];
  int e,i,j,u;
  unsigned int g;
  memset(b,0,sizeof(b));
  i=0;
  e=7;
  g=*(unsigned int*)&f;
  if(g&0x80000000) b[i++]='-',f=-f;
  if((g&0x7f800000)==0) {                          // 0 or denormal
    strcpy(b+i,"0.00000");
    return b;
    }
  if((g&0x7f800000)==0x7f800000) {                 // infinity or NaN
    if((g&0x007fffff)==0) strcpy(b+i,"Inf");
    else                  strcpy(b+i,"NaN");
    return b;
    }
  while(f< 1  ) f*=1e8,e-=8;
  while(f>=1e8) f/=1e8,e+=8;
  if(f<1e4)     f*=1e4,e-=4;
  if(f<1e6)     f*=1e2,e-=2;
  while(f<1e7)  f*=1e1,e-=1;                       // now 1e7<=f<~1e8
  u=(int)(f+50);                                   // for rounding to 6 digits
  if(u>=1e8) u=10000000,e+=1;                      // rounding can overflow; now guaranteed 1e7<=u<1e8
  if(e<-2||e>5) {                                  // use "E" notation
    b[i++]=u/10000000+'0';
    u=(u%10000000)*10;
    b[i++]='.';
    for(j=0;j<5;j++) {
      b[i++]=u/10000000+'0';
      u=(u%10000000)*10;
      }
    b[i++]='E';
    if(e<0) b[i++]='-',e=-e;
    strcpy(b+i,sdec(e));
    return b;
    }
  if(e<0) {
    b[i++]='0';
    b[i++]='.';
    for(j=1;j<-e;j++) b[i++]='0';
    }
  for(j=0;j<6;j++) {
    b[i++]=u/10000000+'0';
    u=(u%10000000)*10;
    if(e==0&&j<5) b[i++]='.';
    e--;
    }
  return b;
  }

