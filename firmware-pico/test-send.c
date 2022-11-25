#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>

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

// receive one character; return -1 if none available
int i1ch() {
  char c;
  if(read(ttyfd,&c,1)==1) return c;
  return -1;
  }

// receive one character, with a timeout in milliseconds
// timeout==-1: never
// return -1 if timeout expires
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

// send one character
void och(int c) {
  for(;;) {
    if(write(ttyfd,&c,1)==1) return;
    usleep(50);
    }
  }

// receive a string, using given timeout on each character
// string is terminated by LF; other non-printing characters ignored
// return -1 if timeout occurred during reading (string still valid)
int wstr(char*s,int n,int timeout) {
  int i,j;
  int rc=0;
  for(i=0;i<n-1;) {
    j=w1ch(timeout);
    if(j==-1) { rc=-1; break; }
    s[i]=j;
    if(s[i]==0x0a) break;
    if(s[i]<0x20) continue;
    i++;
    }
  s[i]=0;
  return rc;
  }

// send a string
void ostr(char*s) {
  int i;
  for(i=0;s[i];i++) och(s[i]);
  }

double timestamp() {
  struct timeval tv;
  gettimeofday(&tv,0);
  return tv.tv_sec+tv.tv_usec/1e6;
  }

int main(int ac,char**av) {
  char s[1000];
  int image_size;
  FILE*fp;
  int i,p;

  if(ac>=2) opentty(av[1]);
  else      opentty("/dev/ttyUSB0");
  usleep(200000);

  if(0) { // simultaneous motors test
    ostr("port 0\r");
    ostr("combi 0 1 0 2 0 3 0 ; select 0 ; plimit .6 ; bias .3 ; pid 0 0 5 s2 0.0027777778 1 5 0 .1 3\r");
    ostr("port 1\r");
    ostr("combi 0 1 0 2 0 3 0 ; select 0 ; plimit .6 ; bias .3 ; pid 0 0 5 s2 0.0027777778 1 5 0 .1 3\r");
  //   ostr("port 0 ; set ramp 0 1 1 0\r"); ostr("port 1 ; set ramp 0 1 1 0\r");
    ostr("port 0 ; set ramp 0 1 1 0 ;  port 1 ; set ramp 0 1 1 0\r");
    sleep(1);
  //  ostr("port 0 ;set ramp 1 0 1 0\r"); ostr("port 1 ;set ramp 1 0 1 0\r");
    ostr("port 0 ;set ramp 1 0 1 0 ; port 1 ;set ramp 1 0 1 0\r");
    sleep(1);
    }

  if(1) { // "selonce" test
    ostr("debug 11 ; port 0 ; plimit .6 ; set .0\r");
    ostr("combi 0 1 0 2 0 3 0\r");
    sleep(.5);
    ostr("port 0 ; select 0 ; selrate -1\r");
    p=0;
    for(;;) {
      i=wstr(s,1000,1000);
      if(s[0]) {
        printf("%.3f: <%s>\n",timestamp(),s);
        }
//      if(!strncmp(s,"P0C0:",5)) {
//        printf("%.3f: sending selonce\n",timestamp());
//        ostr("port 0 ; selonce 0\r");
//        }
      if(i==-1) break;
      }
    }

  for(;;) {
    i=wstr(s,1000,500);
    printf("<%s>\n",s);
    if(i==-1) break;
    }
  closetty();
  return 0;
  }
