#include "common.h"
// #include "system.h"
#include "debug.h"
// #include "gpio.h"
#include "control.h"
// #include "command.h"
#include "ioconv.h"
// #include "hardware.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

#define UART_C uart0
#define UART_C_TXPIN 0
#define UART_C_RXPIN 1
#define UART_C_BAUD 115200

static volatile char ctrlrxbuf[CTRLRXBLEN];
static volatile int ctrlrxhead;
static volatile int ctrlrxtail;

static volatile char ctrltxbuf[CTRLTXBLEN];
static volatile int ctrltxhead;
static volatile int ctrltxtail;

void control_uart_irq() {
  uart_hw_t*u=uart_get_hw(UART_C);
  int b,i,s;
  s=u->fr;
//  uart_putc_raw(uart0,'i');
//  uart_putc_raw(uart0,'0'+(s>>4));
//  uart_putc_raw(uart0,'0'+(s&0x0f));
  if(s&0x80) {                                     // TX empty?
    if(ctrltxhead==ctrltxtail) {                   // buffer empty?
      u->icr=0x20;                              // disable interrupt
    } else {
      u->dr=ctrltxbuf[ctrltxtail];
      ctrltxtail=(ctrltxtail+1)%CTRLTXBLEN;
      }
    }
  if((s&0x10)==0) {                                // RX not empty?
    b=u->dr;                                       // read byte
    i=(ctrlrxhead+1)%CTRLRXBLEN;
    if(i==ctrlrxtail) return;                      // buffer full? discard
    ctrlrxbuf[ctrlrxhead]=b;
    ctrlrxhead=i;
    }
  }

int i1chu() {
  int b;
  if(ctrlrxhead==ctrlrxtail) return -1;
  b=ctrlrxbuf[ctrlrxtail];
  ctrlrxtail=(ctrlrxtail+1)%CTRLRXBLEN;
  return b;
  }

void o1chu(int c) {
  int i;
//  uart_putc_raw(uart0,'c');
  i=(ctrltxhead+1)%CTRLTXBLEN;
  if(i==ctrltxtail) return;                        // buffer full? discard
  ctrltxbuf[ctrltxhead]=c;
  ctrltxhead=i;
//  uart_get_hw(UART_C)->imsc|=0x20;
  irq_set_pending(UART0_IRQ);
  }

void init_control() {
  uart_init(UART_C,UART_C_BAUD);
  gpio_set_function(UART_C_RXPIN,GPIO_FUNC_UART);
  gpio_set_function(UART_C_TXPIN,GPIO_FUNC_UART);
  uart_set_fifo_enabled(UART_C,0);
  irq_set_exclusive_handler(UART0_IRQ,control_uart_irq);
  irq_set_enabled(UART0_IRQ,1);
  uart_set_irq_enables(UART_C,1,1);
  uart_get_hw(UART_C)->ifls=0;
  uart_get_hw(UART_C)->imsc=0x30;
  }
//   RCC->APB1ENR|=APB1CLOCKS_C;
//   RCC->APB2ENR|=APB2CLOCKS_C;
//   ctrlrxhead=ctrlrxtail=0;
//   ctrltxhead=ctrltxtail=0;
//   gpio_afr(UART_C_RXPORT,UART_C_RXPIN,UART_C_RXAFN);
//   gpio_afr(UART_C_TXPORT,UART_C_TXPIN,UART_C_TXAFN);
//   UART_C->BRR=(100000000/UART_C_BAUD+1)/2;
//   UART_C->CR1=0x202c;                              // enable USART, TX and RX, RX interrupt
//   nvic_intena(UART_C_IRQn);
//   onl();
//   ostrnl("Type help <RETURN> for help");
//   cmd_prompt();
//   }
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

void parse_reset() { cbrptr=0; }
int parse_eol() { return cmdbuf[cbrptr]==0; }

void sksp() {                                      // skip spaces
  while(cmdbuf[cbrptr]==' ') cbrptr++;
  }

void skspsc() {                                    // skip spaces and semi-colons
  while(cmdbuf[cbrptr]==' '||cmdbuf[cbrptr]==';') cbrptr++;
  }

static int isdigit(int c) { return c>='0'&&c<='9'; }

// attempt to parse an unsigned integer
// return 1 for success, writing result to *p, and advance cbrptr
int parseuint(unsigned int*p) {
  int i=cbrptr;
  unsigned int v=0;
  if(!isdigit(cmdbuf[i])) return 0;
  while(isdigit(cmdbuf[i])) v=v*10+cmdbuf[i++]-'0';
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
  *p=neg?-(int)u:(int)u;
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
      if(!isdigit(cmdbuf[cbrptr])) break;
      m=(m+5)/10;                                  // m goes through negative powers of 10 Q24
      u+=(cmdbuf[cbrptr++]-'0')*m;
      }
    v+=(u+0x80)>>8;                                // rounded and shifted to Q16
    }
  *p=neg?-(int)v:(int)v;
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
    if(!isdigit(cmdbuf[i])) return 0;
    while(isdigit(cmdbuf[i])) {
      j++;                                         // count digits seen
      if(u<100000000) u=u*10+cmdbuf[i]-'0';
      else            m0*=10;
      i++;
      }
    }
  if(cmdbuf[i]=='.') {
    i++;
    while(isdigit(cmdbuf[i])) {
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

int parsepvfmt(unsigned int*p) {
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

// check for match against given string
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

void proc_ctrl() {
  int u;
  for(;;) {
    u=i1ch();
    if(u==-1) return;
    if(u==0x0d) {
      onl();
      cmdbuf[cbwptr]=0;
//      proc_cmd(); !!!
      cbwptr=0;
      continue;
      }
    if(u==0x08||u==0x7f) {
      if(cbwptr<1) continue;
      cbwptr--;
      o1ch(0x08); osp(); o1ch(0x08);
      continue;
      }
    if(cbwptr==CMDBUFLEN) continue;                // buffer full?
    o1ch(u);
    cmdbuf[cbwptr++]=u;
    }
  }
