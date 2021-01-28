// port 1 ; set triangle 0 .1 1 0
// port 1 ; set sine 0 .1 1 0
// port 1 ; set square 0 1 2 0
// example for position PID:
// port 1 ; pid 1 3 0 s2 0.0027777778 1 15 0 .1 3
// set square 0 .3 5 0
// set triangle 0 .3 .5 0
// (non-working) example for speed PID:
// port 1 ; pid 1 1 0 s1 1 0 0.1 0 0 3

#include "stm32f413xx.h"
#include <string.h>
#include "common.h"
#include "debug.h"
#include "system.h"
#include "gpio.h"
#include "ioconv.h"
#include "adc.h"
#include "timer.h"
#include "hardware.h"
#include "ports.h"
#include "leds.h"
#include "control.h"
#include "device.h"
#include "pwm_pid.h"
#include "message.h"

// =============================== MAIN STATE MACHINE ==========================

// timer 0: NACK timing during data phase
// timer 1: watchdog during setup and data phases
// timer 2: PID update
#define NTIMERS 3

// counter 0: signature bit generation
// counter 1: how many times we have seen this passive ID consecutively (up to 5)
#define NCOUNTERS 2

void go() {
  int i,j,u;
  int id;
  int state[NPORTS];
  int delay[NPORTS];
  int timers[NPORTS][NTIMERS];
  int counters[NPORTS][NCOUNTERS];
  unsigned int t0,deltat;
  for(i=0;i<NPORTS;i++) {
    state[i]=0;
    delay[i]=0;
    device_init(i);
    port_uartoff(i);
    }
  memset(timers,0,sizeof(timers));
  memset(counters,0,sizeof(counters));
  t0=tick;
  for(;;) {
    deltat=tick-t0;                                // milliseconds since we were last here
    if(deltat>2) {                                 // normally this will be 0 or 1
      ostr("deltat="); o8hex(deltat); onl();
      }
    t0+=deltat;
    for(i=0;i<NPORTS;i++) {
      struct devinfo*d=devinfo+i;
      struct porthw*p=porthw+i;
      struct portinfo*q=portinfo+i;
      if(deltat) {
        delay[i]-=deltat;
        for(j=0;j<NTIMERS;j++) timers[i][j]+=deltat;
        }
      if(delay[i]>0) continue;                     // skip processing while we are delaying
      delay[i]=0;
      switch(state[i]) {
    case 0:
        d->signature=0;
        counters[i][0]=0;
        gpio_inpu(p->port_d5,p->pin_d5);           // enable pull-ups to mitigate effect of RS-485 driver output resistance
        gpio_in  (p->port_rx,p->pin_rx);
        state[i]++;
    case 1:
        switch(counters[i][0]/3) {
      case 0:
        gpio_out1(p->port_tx,p->pin_tx);
        gpio_out0(p->port_en,p->pin_en);           // enable pin 5 driver, pull high
        break;
      case 1:
        gpio_out1(p->port_en,p->pin_en);           // disable pin 5 driver
        break;
      case 2:
        gpio_out0(p->port_tx,p->pin_tx);
        gpio_out0(p->port_en,p->pin_en);           // enable pin 5 driver, pull low
        break;
          }
        switch(counters[i][0]%3) {
      case 0:
        gpio_out1(p->port_d6,p->pin_d6);
        break;
      case 1:
        gpio_inpu(p->port_d6,p->pin_d6);
        break;
      case 2:
        gpio_out0(p->port_d6,p->pin_d6);
        break;
          }
        delay[i]=2;                                // at least 2ms for settling; pin 5 stays in each state for 6ms, so cannot
                                                   // generate a valid character
        state[i]++;
        break;
    case 2:
        d->signature|=GPIO_READ(p->port_d5,p->pin_d5)<< counters[i][0];
        d->signature|=GPIO_READ(p->port_rx,p->pin_rx)<<(counters[i][0]+12);
        counters[i][0]++;
        if(counters[i][0]<9) state[i]--;
        else                 state[i]++;
        break;
    case 3:
        gpio_out1(p->port_en,p->pin_en);           // disable driver
        gpio_in  (p->port_d5,p->pin_d5);
        gpio_in  (p->port_d6,p->pin_d6);
DEB_SIG         { o1ch('P'); o1hex(i); ostr(": D5 signature="); o8hex(d->signature); }
        if((d->signature&0xfff)==0x024)            // for ID 11 the logic levels on the RX buffer input are marginal
          d->signature &=0xfff;                    // so ignore it
        switch(d->signature) {
      default:
      case 0x000f303f: id= 0; break;               // nothing connected
      case 0x00038007: id= 1; break;               // System medium motor
      case 0x000381ff: id= 2; break;               // System train motor
      case 0x001c7000: id= 3; break;               // System turntable motor
      case 0x00038000: id= 4; break;               // general PWM/third party
      case 0x0003f01f:                             // button/touch sensor not pressed
      case 0x0003f00f: id= 5; break;               // button/touch sensor pressed
      case 0x001ff1ff: id= 6; break;               // Technic large motor
      case 0x001c7007: id= 7; break;               // Technic XL motor (note that some have active ID!)
      case 0x000e3007: id= 8; break;               // simple lights
      case 0x000fb1ff: id= 9; break;               // Future lights 1
      case 0x000c3000: id=10; break;               // Future lights 2
      case 0x00000024: id=11; break;               // System future actuator (train points) (signature is 0x00061024)
      case 0x0003803f: id=99; break;               // FLOAT/GND: potentially an active ID
          }
DEB_SIG        { ostr(" id="); odec(id); onl(); }
        if(id==99) {
          state[i]=90;                             // attempt to decode active ID
          break;
          }
        state[i]=0;
        delay[i]=100;
        if((d->id!=0&&id!=d->id) || id==0) {       // non-constant ID: reset
          if(d->connected) { o1ch('P'); o1hex(i); ostrnl(": disconnected"); }
          d->connected=0;
          d->id=0;
          counters[i][1]=0;
          break;
          }
        d->id=id;                                  // set provisional ID
        if(counters[i][1]<5) {                     // attempt to get 5 matching IDs quickly
          counters[i][1]++;
          delay[i]=0;
          break;
          }
        if(d->connected==0) { o1ch('P'); o1hex(i); ostr(": connected to passive ID "); odec(id); onl(); }
        d->connected=1;                            // we have 5 matching IDs, so mark as connected
        if(id==5) {                                // button/touch sensor?
          delay[i]=0;                              // then reinterrogate quickly to respond better to presses
          d->buttonpressed=(d->signature==0x003f00f);
          }
        break;

// ============================== ACTIVE ID PROCESSING =========================

    case 90:                                       // here we may have an active ID to detect
        gpio_in  (p->port_d5,p->pin_d5);
        gpio_inpu(p->port_d6,p->pin_d6);
        timers[i][1]=0;
        state[i]++;
        break;
    case 91:
        if(GPIO_READ(p->port_d6,p->pin_d6)==1) {   // look to see RXD held low for 100ms
          state[i]=0;
          break;
          }
        if(timers[i][1]>=100) state[i]++;
        break;
    case 92:
        port_uarton(i);
        o1ch('P'); o1hex(i); ostrnl(": connecting to active device");
        device_init(i);                            // reset all devinfo
        delay[i]=10;
        state[i]=100;
        timers[i][1]=0;
        break;

// ============================= ACTIVE ID: SETUP PHASE ========================

    case 100:                                      // setup phase
        u=proc_setupmessages(i);
        if(u==-1) {                                // idle?
          if(q->framingerrors>0) {
            o1ch('P'); o1hex(i); ostrnl(": framing error: disconnecting");
            q->framingerrors=0;
            }
          if(q->checksumerrors>0) {
            o1ch('P'); o1hex(i); ostrnl(": checksum error: disconnecting");
            q->checksumerrors=0;
            }
          if(timers[i][1]>2000) {
            o1ch('P'); o1hex(i); ostrnl(": timeout during setup phase: disconnecting");
            state[i]=0;
            }
          }
        else timers[i][1]=0;
        proc_ctrl();
        if(u!=0x04) break;                         // not an ACK: keep reading messages
        state[i]++;
        break;
    case 101:                                      // we have received an ACK
        device_dump(i);                            // dump device data and pause
        if(d->type<0) {                            // no TYPE information?
          state[i]=0;                              // reset and try again
          break;
          }
        delay[i]=10;
        state[i]++;
        break;
    case 102:                                      // send the baud rate back; pause
        device_sendint(i,0x42,-1,d->baud);
        delay[i]=100;
        state[i]++;
        break;
    case 103:
        device_sendsys(i,4);                       // send ACK; pause
        delay[i]=30;
        state[i]++;
        break;
    case 104:
        port_setbaud(i,d->baud);                   // switch baud rate
        delay[i]=100;
        state[i]++;
        break;
    case 105:
        timers[i][0]=0;
        o1ch('P'); o1hex(i); ostr(": connected to active ID "); o2hex(d->type); onl();
        d->connected=1;
        state[i]=200;
        break;

// ============================= ACTIVE ID: DATA PHASE =========================

    case 200:                                      // process data phase messages
        u=proc_datamessages(i);
        if(u==-1) {                                // idle?
          if(q->framingerrors+q->checksumerrors>5) {
            o1ch('P'); o1hex(i); ostr(": too many errors (");
            odec(q->framingerrors); o1ch('+');
            odec(q->checksumerrors); ostrnl("): disconnecting");
            state[i]=0;
            }
          if(timers[i][1]>500) {
            o1ch('P'); o1hex(i); ostrnl(": timeout during data phase: disconnecting");
            state[i]=0;
            }
          }
        else timers[i][1]=0;                       // reset watchdog
        if(timers[i][0]>=100) {                    // send a NACK every 100ms
          timers[i][0]-=100;
          state[i]++;
          }
        break;
    case 201:
        device_sendsys(i,2);                       // send NACK
        state[i]--;
        break;
        }
      }
    for(i=0;i<NPORTS;i++) {
      if(timers[i][2]>=PWM_UPDATE) {             // PID update
        timers[i][2]-=PWM_UPDATE;
        proc_pwm(i);
        }
      }
    proc_ctrl();
    }
  }

// =================================== main() ==================================

int main() {
  init_data_bss();
  init_sys();
  init_gpio();
  init_adc();
  init_timer();
  init_ports();
  init_control();

  go();

//   test4();
// 
//   ostrnl("P for PWM test");
//   ostrnl("G for ID test");
// 
//   for(;;) {
//     switch(w1ch()) {
//   case 'p': case 'P': test4(); break;
//   case 'g': case 'G': go(); break;
//       }
//     }
// 
//   ostrnl("Done");
  return 0;
  }
