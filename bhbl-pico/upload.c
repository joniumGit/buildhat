#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>

#define IMAGEBUFSIZE (240*1024)
#define SIGSIZE 64
unsigned char imagebuf[IMAGEBUFSIZE+1];
unsigned char signature[SIGSIZE];

#define BAUD B115200

int ttyfd;

void opentty(char*dev) {
  struct termios ttyopt;

  ttyfd=open(dev,O_RDWR|O_NOCTTY|O_NONBLOCK);
  if(ttyfd<0) {fprintf(stderr,"Failed to open %s\n",dev); exit(4);}
  tcgetattr(ttyfd,&ttyopt);
  cfsetispeed(&ttyopt,BAUD);
  cfsetospeed(&ttyopt,BAUD);
  cfmakeraw(&ttyopt);
  tcsetattr(ttyfd,TCSANOW,&ttyopt);
  tcflush(ttyfd,TCIOFLUSH);
  }

void closetty() {close(ttyfd);}

int i1ch() {
  char c;
  if(read(ttyfd,&c,1)==1) return c;
  return -1;
  }

// wait, with a timeout in milliseconds
// timeout==-1: never
int w1ch(int timeout) {
  int c;
  for(;;) {
    c=i1ch();
    if(c!=-1) return c;
    if(timeout>0) timeout--;
    if(timeout==0) return -1;
    usleep(1000);
    }
  }

void och(int c) {
  for(;;) {
    if(write(ttyfd,&c,1)==1) return;
    usleep(50);
    }
  }

void wstr(char*s,int n,int timeout) {
  int i,j;
  for(i=0;i<n-1;) {
    j=w1ch(timeout);
    if(j==-1) break;
    s[i]=j;
    if(s[i]==0x0a) break;
    if(s[i]<0x20) continue;
    i++;
    }
  s[i]=0;
  }

void ostr(char*s) {
  int i;
  for(i=0;s[i];i++) och(s[i]);
  }

unsigned int checksum(unsigned char*p,int l) {
  unsigned int u=1;
  int i;
  for(i=0;i<l;i++) {
    if((u&0x80000000)!=0) u=(u<<1)^0x1d872b41;
    else                  u= u<<1;
    u^=p[i];
    }
  return u;
  }

int getprompt() {
  char s[100];
  int p=0,t=0,u=0;
  for(;;) {
    wstr(s,100,10);
    if(strlen(s)==0) {
      if(p==1&&t==10) return 1;              // wait for 10*timeout with no data
      if(p==0&&t==10) {                      // no prompt, so send <RETURN>
        och(0x0d);
        t=0;
        u++;
        if(u==10) return 0;
        }
      t++;
      continue;
      }
    if(!strcmp(s,"BHBL> ")) p=1;
    else {
      p=0;
      fprintf(stderr,"<%s>\n",s);
      }
    t=0;
    }
  }



int main(int ac,char**av) {
  char s[1000];
  int image_size;
  FILE*fp;
  int i;

  if(ac<3) {
    fprintf(stderr,"Usage: %s <tty device> <image file> <signature file>\n",av[0]);
    return 4;
    }
  opentty(av[1]);

  fp=fopen(av[2],"rb");
  if(!fp) {
    fprintf(stderr,"Failed to open image file %s\n",av[2]);
    return 16;
    }
  image_size=fread(imagebuf,1,IMAGEBUFSIZE+1,fp);
  if(image_size<1) {
    fprintf(stderr,"Error reading image file %s\n",av[2]);
    return 16;
    }
  if(image_size>IMAGEBUFSIZE) {
    fprintf(stderr,"Image file %s is too large (maximum %d bytes)\n",av[2],IMAGEBUFSIZE);
    return 16;
    }

  fp=fopen(av[3],"rb");
  if(!fp) {
    fprintf(stderr,"Failed to open signature file %s\n",av[3]);
    return 16;
    }
  i=fread(signature,1,SIGSIZE,fp);
  if(i!=SIGSIZE) {
    fprintf(stderr,"Error reading signature file %s\n",av[3]);
    return 16;
    }
  if(!getprompt()) goto ew0;
  ostr("clear\r");
  if(!getprompt()) goto ew0;
  sprintf(s,"load %d %u\r",image_size,checksum(imagebuf,image_size));
  ostr(s);
  usleep(100000);
  ostr("\x02");
  for(i=0;i<image_size;i++) och(imagebuf[i]);
  ostr("\x03\r");
  if(!getprompt()) goto ew0;
  sprintf(s,"signature %d\r",SIGSIZE);
  ostr(s);
  usleep(100000);
  ostr("\x02");
  for(i=0;i<SIGSIZE;i++) och(signature[i]);
  ostr("\x03\r");
  if(!getprompt()) goto ew0;
  ostr("reboot\r");
  for(;;) {
    i=w1ch(-1);
    fputc(i,stderr);
    }
  closetty();
  return 0;

ew0:
  fprintf(stderr,"Failed to communicate with Build HAT\n");
  closetty();
  return 16;
  }
