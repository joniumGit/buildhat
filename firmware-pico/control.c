#include <ctype.h>
#include "common.h"
#include "debug.h"
#include "control.h"
#include "command.h"
#include "ioconv.h"
#include "hardware.h"
#include "timer.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

int echo=0;

static volatile char ctrlrxbuf[CTRLRXBLEN];
static volatile int ctrlrxhead;
static volatile int ctrlrxtail;

static volatile char ctrltxbuf[CTRLTXBLEN];
static volatile int ctrltxhead;
static volatile int ctrltxtail;

// interrupt service routine for the control UART
void control_uart_irq() {
  uart_hw_t*u=uart_get_hw(UART_C);
  int b,i,s;
  s=u->fr;
  if(s&0x80) {                                     // TX empty?
    if(ctrltxhead==ctrltxtail) {                   // buffer empty?
      u->icr=0x20;                                 // disable interrupt
    } else {
      u->dr=ctrltxbuf[ctrltxtail];                 // extract a character from the circular TX buffer
      ctrltxtail=(ctrltxtail+1)%CTRLTXBLEN;
      }
    }
  if((s&0x10)==0) {                                // RX not empty?
    b=u->dr;                                       // read byte
    i=(ctrlrxhead+1)%CTRLRXBLEN;
    if(i==ctrlrxtail) return;                      // buffer full? discard
    ctrlrxbuf[ctrlrxhead]=b;                       // insert in circular RX buffer
    ctrlrxhead=i;
    }
  }

// get one received character; -1 if none waiting
int i1chu() {
  int b;
  if(ctrlrxhead==ctrlrxtail) return -1;            // buffer empty?
  b=ctrlrxbuf[ctrlrxtail];                         // extract a character from the circular RX buffer
  ctrlrxtail=(ctrlrxtail+1)%CTRLRXBLEN;
  return b;
  }

// send one character
void o1chu(int c) {
  int i;
  i=(ctrltxhead+1)%CTRLTXBLEN;
  if(i==ctrltxtail) return;                        // buffer full? discard
  ctrltxbuf[ctrltxhead]=c;                         // insert in circular TX buffer
  ctrltxhead=i;
  irq_set_pending(UART0_IRQ);                      // make sure an interrupt occurs to process this character
  }

// return whether the circular TX buffer is less than half full
int ctrl_ospace() {
  int i;
  i=ctrltxhead-ctrltxtail;
  if(i<0) i+=CTRLTXBLEN;               // i now has number of characters in output buffer
  return i<CTRLTXBLEN/2;               // less than half full?
  }

// initialise the control interface
void init_control() {
  irq_set_enabled(UART0_IRQ,0);
  ctrlrxhead=ctrlrxtail=0;             // empty buffers
  ctrltxhead=ctrltxtail=0;
  uart_init(UART_C,UART_C_BAUD);
  gpio_set_function(UART_C_RXPIN,GPIO_FUNC_UART);
  gpio_set_function(UART_C_TXPIN,GPIO_FUNC_UART);
  uart_set_fifo_enabled(UART_C,0);
  irq_set_exclusive_handler(UART0_IRQ,control_uart_irq);
  uart_get_hw(UART_C)->ifls=0;
  uart_get_hw(UART_C)->imsc=0x30;
  uart_get_hw(UART_C)->dr;             // read any character that may be in the UART's buffer
  uart_get_hw(UART_C)->icr=0x7ff;
  irq_set_enabled(UART0_IRQ,1);
  onl();
  ostrnl("Type help <RETURN> for help");
  cmd_prompt();
  do wait_ticks(10); while(i1ch()!=-1); // discard any spurious characters received at the start
  }

// wait for a character
int w1ch() {
  int u;
  do u=i1ch(); while(u==-1);
  return u;
  }

// ============================= CONTROL TERMINAL ==============================

static char cmdbuf[CMDBUFLEN+1];
static int cbwptr=0;
static int cbrptr;

// reset the parser
void parse_reset() { cbrptr=0; }

// are we at the end of the input line?
int parse_eol() { return cmdbuf[cbrptr]==0; }

void sksp() {                                      // skip spaces
  while(cmdbuf[cbrptr]==' ') cbrptr++;
  }

void skspsc() {                                    // skip spaces and semi-colons
  while(cmdbuf[cbrptr]==' '||cmdbuf[cbrptr]==';') cbrptr++;
  }

// attempt to parse a hexadecimal integer
// return 1 for success, writing result to *p, and advance cbrptr
int parsehex(unsigned int*p) {
  int i=cbrptr,t;
  unsigned int v=0;
  if(!isxdigit((UC)cmdbuf[i])) return 0;
  while(isxdigit((UC)cmdbuf[i])) {
    t=cmdbuf[i++];
    if(islower(t)) t-='a'-'A';
    if(isalpha(t)) t-='A'-10;
    else           t-='0';
    v=v*16+t;
    }
  cbrptr=i;
  sksp();
  *p=v;
  return 1;
  }

// attempt to parse an unsigned integer
// return 1 for success, writing result to *p, and advance cbrptr
int parseuint(unsigned int*p) {
  int i=cbrptr;
  unsigned int v=0;
  if(!isdigit((UC)cmdbuf[i])) return 0;
  while(isdigit((UC)cmdbuf[i])) v=v*10+cmdbuf[i++]-'0';
  cbrptr=i;
  sksp();
  *p=v;
  return 1;
  }

// attempt to parse a signed integer
// return 1 for success, writing result to *p, and advance cbrptr
int parseint(int*p) {
  int i=cbrptr;
  int neg=0;
  unsigned int u;
  if     (cmdbuf[cbrptr]=='+') cbrptr++;
  else if(cmdbuf[cbrptr]=='-') cbrptr++,neg=1;
  if(!parseuint(&u)) {
    cbrptr=i;                                      // restore cbrptr if we failed to find an integer
    return 0;
    }
  *p=neg?-(int)u:(int)u;                           // apply sign
  return 1;
  }

// attempt to parse a signed decimal value
// return 1 for success, writing result as Q16 to *p, and advance cbrptr
int parseq16(int*p) {
  int i=cbrptr;
  int neg=0;
  unsigned int u,m;
  int v=0;
  if     (cmdbuf[cbrptr]=='+') cbrptr++;
  else if(cmdbuf[cbrptr]=='-') cbrptr++,neg=1;
  if(cmdbuf[cbrptr]!='.') {
    if(!parseuint(&u)) {                           // capture the integer part
      cbrptr=i;                                    // restore cbrptr if we failed to find an integer
      return 0;
      }
    v=u<<16;
    }
  if(cmdbuf[cbrptr]=='.') {
    cbrptr++;
    u=0;
    m=0x01000000;                                  // 1 Q24
    for(;;) {
      if(!isdigit((UC)cmdbuf[cbrptr])) break;
      m=(m+5)/10;                                  // m goes through negative powers of 10 Q24
      u+=(cmdbuf[cbrptr++]-'0')*m;
      }
    v+=(u+0x80)>>8;                                // rounded and shifted to Q16
    }
  *p=neg?-(int)v:(int)v;                           // apply sign
  sksp();
  return 1;
  }

// attempt to parse a float
// return 1 for success, writing result to *p, and advance cbrptr
int parsefloat(float*p) {
  int i=cbrptr,j=0;
  unsigned int u;
  float m0,m1;
  u=0; m0=1; m1=1;
  if     (cmdbuf[i]=='+') i++;
  else if(cmdbuf[i]=='-') i++,m0=-1;
  if(cmdbuf[i]!='.') {
    if(!isdigit((UC)cmdbuf[i])) return 0;
    while(isdigit((UC)cmdbuf[i])) {
      j++;                                         // count digits seen
      if(u<100000000) u=u*10+cmdbuf[i]-'0';
      else            m0*=10;
      i++;
      }
    }
  if(cmdbuf[i]=='.') {
    i++;
    while(isdigit((UC)cmdbuf[i])) {
      j++;                                         // count digits seen
      if(u<100000000) u=u*10+cmdbuf[i]-'0',m1*=10;
      i++;
      }
    }
  if(j==0) return 0;                               // must have at least one digit
  *p=(float)u*m0/m1;
  cbrptr=i;
  sksp();
  return 1;
  }

// attempt to parse a format specifier
// return 1 for success, writing code to *p, and advance cbrptr
int parsefmt(int*p) {
  if     (cmdbuf[cbrptr]=='u'&&cmdbuf[cbrptr+1]=='1') *p=0x001;
  else if(cmdbuf[cbrptr]=='s'&&cmdbuf[cbrptr+1]=='1') *p=0x101;
  else if(cmdbuf[cbrptr]=='u'&&cmdbuf[cbrptr+1]=='2') *p=0x002;
  else if(cmdbuf[cbrptr]=='s'&&cmdbuf[cbrptr+1]=='2') *p=0x102;
  else if(cmdbuf[cbrptr]=='u'&&cmdbuf[cbrptr+1]=='4') *p=0x004;
  else if(cmdbuf[cbrptr]=='s'&&cmdbuf[cbrptr+1]=='4') *p=0x104;
  else if(cmdbuf[cbrptr]=='f'&&cmdbuf[cbrptr+1]=='4') *p=0x204;
  else return 0;
  cbrptr+=2;
  sksp();
  return 1;
  }

// check for match against given string, allowing initial substring matches
// return 1 if a match, and advance cbrptr
int strmatch(char*s) {
  int i,j;
  sksp();
  for(i=cbrptr,j=0;i<=CMDBUFLEN;i++,j++) {
    if(j>0) {                                      // allow partial matches
      if(cmdbuf[i]==' ') { cbrptr=i+1; sksp(); return 1; }
      if(cmdbuf[i]==0)   { cbrptr=i;   sksp(); return 1; }
      if(cmdbuf[i]==';') { cbrptr=i+1; sksp(); return 1; }
      }
    if(s[j]==0) return 0;
    if(s[j]!=cmdbuf[i]) return 0;
    }
  return 0;
  }

// check the control interface and carry out any required actions
void proc_ctrl() {
  int u;
  for(;;) {
    u=i1ch();
    if(u==-1) return;
    if(u==0x0d) {                                // carriage return
      onl();
      cmdbuf[cbwptr]=0;
      if(cmdbuf[0]!='#') proc_cmd();             // "#" introduces a comment: discard it; otherwise process the command
      cbwptr=0;
      continue;
      }
    if(u==0x08||u==0x7f) {                       // delete/backspace
      if(cbwptr<1) continue;
      cbwptr--;
      if(echo) { o1ch(0x08); osp(); o1ch(0x08); }
      continue;
      }
    if(cbwptr==CMDBUFLEN) continue;                // buffer full?
    if(echo) o1ch(u);
    cmdbuf[cbwptr++]=u;
    }
  }
