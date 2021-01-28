#include "common.h"
#include "debug.h"
#include "gpio.h"
#include "control.h"
#include "ioconv.h"
#include "hardware.h"
#include "ports.h"
#include "timer.h"

void proc_debug(int u) {
  int i;
  static int w=0;
  static int porton[4]={0,1,0,0};
  if(u==0) u=i1ch();
  if(u==-1) return;
  switch(u) {
case '0': if(w>-100) w-=1; break;
case '1': if(w< 100) w+=1; break;
case '2': port_initpwm(PWM_PERIOD_DEFAULT  ); ostrnl("PWM frequency ~  6kHz"); break;
case '3': port_initpwm(PWM_PERIOD_DEFAULT/2); ostrnl("PWM frequency ~ 12kHz"); break;
case '4': port_initpwm(PWM_PERIOD_DEFAULT/4); ostrnl("PWM frequency ~ 24kHz"); break;
case 'A': case 'a': porton[0]=!porton[0]; break;
case 'B': case 'b': porton[1]=!porton[1]; break;
case 'C': case 'c': porton[2]=!porton[2]; break;
case 'D': case 'd': porton[3]=!porton[3]; break;
case 'R': case 'r': w=0; break;
default : return;
    }
  for(i=0;i<4;i++) {
    if(porton[i]) {
      ostr("√"); osp();
      port_set_pwm(i,(float)w/100);
    } else {
      ostr("×"); osp();
      port_set_pwm(i,0);
      }
    }
  for(i=0;i<4;i++) {
    o4hex(*(porthw[i].tccr1)); osp();
    o4hex(*(porthw[i].tccr2)); osp(); osp();
    }
  osdec(w); ostr("% "); onl();
  }

void test0() {
  int i;
  unsigned int t;
  port_uarton(1);
  porthw[1].uart->CR1&=~0xa0;                      // disable interrupts
  for(;;) {
    t=tick;
    i=port_waitch(1);
    if(tick-t>100) onl();                          // newline if there was a pause
    else           osp();
    o2hex(i);
    }
  }

void test1() {
  int c,i,l,m,t;
  port_uarton(1);
  porthw[1].uart->CR1&=~0xa0;                      // disable interrupts

  for(;;) {
    i=port_waitch(1); c=i;
//    o2hex(i); osp();
//    if(i==0) onl();
//    continue;
    t=(i>>6)&3;                                    // message type
    l=1<<((i>>3)&7);                               // log of payload length
    m=i&7;                                         // message subtype / mode
    switch(t) {
  case 0: // "system message"
      if(i==0) ostrnl("SYNC");
      else if(i==1||i==3||i==7) ostrnl("SYNC r");
      else if(i==2) ostrnl("NACK");
      else if(i==4) ostrnl("ACK");
      else if(m==6) ostrnl("ESC");
      else if(m==5) {
        ostr("PRG ");
        i=port_waitch(1); c^=i;
        o2hex(i); osp();
dumpbytes:
        while(l>0) {
          i=port_waitch(1); c^=i;
          o2hex(i); osp();
          l--;
          }
        i=port_waitch(1);
        o1ch('='); o2hex(i);
        if((i^c)==0xff) ostrnl(" √");
        else          ostrnl(" CS FAIL");
        }
      else o1ch('<'),o2hex(i),o1ch('>'),onl();
      break;
  case 1: // "command message"
      if(m==0) { ostr("TYPE "    ); goto dumpbytes; }
      if(m==1) { ostr("MODES "   ); goto dumpbytes; }
      if(m==2) { ostr("SPEED "   ); goto dumpbytes; }
      if(m==3) { ostr("SELECT "  ); goto dumpbytes; }
      if(m==4) { ostr("WRITE "   ); goto dumpbytes; }
      if(m==5) { ostr("CMD5 "    ); goto dumpbytes; }
      if(m==6) { ostr("MODESETS "); goto dumpbytes; }
      if(m==7) { ostr("VERSION " ); goto dumpbytes; }
      break;
  case 2: // "info message"
      ostr("INFO mode "); o1hex(m);
      i=port_waitch(1); c^=i;
      ostr(" cmd "); o2hex(i);
      osp();
      goto dumpbytes;
    default:
      o1ch('['),o2hex(i),o1ch(']');
      }
    continue;
    }
  }

void test2() {
  int i,j,pn,u;
  struct porthw*p;

  pn=1;
  p=porthw+pn;
  for(;;) {
    ostrnl("D5  D6");
    ostrnl("HZL HZL");
    onl();
    gpio_inpu(p->port_d5,p->pin_d5);               // emable pull-ups to mitigate effect of RS-485 driver output resistance
    gpio_inpu(p->port_d6,p->pin_d6);
    for(j=0;;j++) {
      GPIOD->ODR|=8;
      u=0;
      for(i=0;i<3;i++) {
        switch(i) {
      case 0:
          gpio_out1(p->port_tx,p->pin_tx);
          gpio_out0(p->port_en,p->pin_en);         // enable driver, pull high
          break;
      case 1:
          gpio_in  (p->port_tx,p->pin_tx);
          gpio_out1(p->port_en,p->pin_en);         // disable driver
          break;
      case 2:
          gpio_out0(p->port_tx,p->pin_tx);
          gpio_out0(p->port_tx,p->pin_tx);
          gpio_out0(p->port_en,p->pin_en);         // enable driver, pull low
          break;
          }
        wait_ticks(5);
        o1hex(GPIO_READ(p->port_d5,p->pin_d5));
        u|=GPIO_READ(p->port_d5,p->pin_d5)<<i;
        }
      osp();
      GPIOD->ODR&=~8;
      for(i=0;i<3;i++) {
        switch(i) {
      case 0: gpio_out1(p->port_d6,p->pin_d6); break;
      case 1: gpio_in  (p->port_d6,p->pin_d6); break;
      case 2: gpio_out0(p->port_d6,p->pin_d6); break;
          }
        wait_ticks(5);
        o1hex(GPIO_READ(p->port_d6,p->pin_d6));
        u|=GPIO_READ(p->port_d6,p->pin_d6)<<i;
        u|=port_state56(pn)<<(i*2);
        if(i%3==2) osp();
        }
      gpio_out1(p->port_en,p->pin_en);             // disable driver
      gpio_in  (p->port_d6,p->pin_d6);
      o2hex(u); osp();
      onl();
      }
    onl();
    }
  }

void test3() {
  int i,j,pn,u;
  struct porthw*p;

  pn=1;
  p=porthw+pn;
  for(;;) {
    ostrnl("D6: == H ===  == Z ===  == L ===");
    ostrnl("D5: H  Z  L   H  Z  L   H  Z  L ");
    ostrnl("    65 65 65  65 65 65  65 65 65");
    onl();
    for(j=0;j<10;j++) {
      u=0;
      ostr("    ");
      for(i=0;i<9;i++) {
        switch(i%3) {
      case 0:
          gpio_out1(p->port_tx,p->pin_tx);
          gpio_out0(p->port_en,p->pin_en);         // enable driver
          break;
      case 1:
          gpio_in  (p->port_tx,p->pin_tx);
          gpio_out1(p->port_en,p->pin_en);         // disable driver
          break;
      case 2:
          gpio_out0(p->port_tx,p->pin_tx);
          gpio_out0(p->port_tx,p->pin_tx);
          gpio_out0(p->port_en,p->pin_en);         // enable driver
          break;
          }
        switch(i/3) {
      case 0: gpio_out1(p->port_d6,p->pin_d6); break;
      case 1: gpio_in  (p->port_d6,p->pin_d6); break;
      case 2: gpio_out0(p->port_d6,p->pin_d6); break;
          }
        wait_ticks(100);
        o1hex(GPIO_READ(p->port_d6,p->pin_d6));
        o1hex(GPIO_READ(p->port_d5,p->pin_d5));
        osp();
        u|=port_state56(pn)<<(i*2);
        if(i%3==2) osp();
        }
      gpio_out1(p->port_en,p->pin_en);             // disable driver
      gpio_in  (p->port_d6,p->pin_d6);
      o8hex(u); osp();
      onl();
      }
    onl();
    }
  }

void test4() {
  ostrnl("PWM test");
  ostrnl("0: decrement PWM duty cycle percentage");
  ostrnl("1: increment PWM duty cycle percentage");
  ostrnl("R: reset PWM duty cycle to 0");
  ostrnl("2: set PWM frequency to ~  6kHz (period=163.84μs)");
  ostrnl("3: set PWM frequency to ~ 12kHz (period= 81.92μs)");
  ostrnl("4: set PWM frequency to ~ 24kHz (period= 40.96μs)");
  ostrnl("A: enable/disable port A");
  ostrnl("B: enable/disable port B");
  ostrnl("C: enable/disable port C");
  ostrnl("D: enable/disable port D");
  ostrnl("        ----A----  ----B----  ----C----  ----D----");
  ostrnl("A B C D CCR1 CCR2  CCR1 CCR2  CCR1 CCR2  CCR1 CCR2");
  proc_debug('R');
  for(;;)
    proc_debug(0);
  }


