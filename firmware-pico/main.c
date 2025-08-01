// Copyright (c) 2025 Raspberry Pi (Trading) Ltd.
//
// SPDX-License-Identifier: BSD-3-Clause

/*
Simple examples e.g. to drive train lights
==========================================

port 1 ; set triangle 0 .1 1 0
port 1 ; set sine 0 .1 1 0
port 1 ; set square 0 1 2 0
port 1 ; set pulse 0.2 .03 2 0

Examples
========

These examples assume you have a small motor connected to each of ports 0 and 1.
In all cases set "plimit 1" and (if wanted) "echo 1".

1. Simple position PID

port 0
combi 0 1 0 2 0 3 0 ; select 0
pwmparams .6 0.01
pid 0 0 5 s2    0.0027777778 1    5 0.0 0.2   3 0.005
set square 0 1 3 0

2. Simple speed PID using speed sensor

port 0
combi 0 1 0 2 0 3 0 ; select 0
pwmparams .6 0.01
pid 0 0 0 s1    1 0    0.01 0.02 0.0001    100 .01
set triangle -100 100 3 0

3. Speed PID using derivative of position sensor

port 0
combi 0 1 0 2 0 3 0 ; select 0 
pwmparams .6 0.01
pid_diff 0 0 5 s2    0.0027777778 1    0 2.5 0   .4 0.01
set triangle -3 3 3 0

4. Motor 0 position tracks position of motor 1

# set up port 1 motor to get all the variables
port 1 ; coast ; combi 0 1 0 2 0 3 0 ; select 0
port 0 ; combi 0 1 0 2 0 3 0 ; select 0 ; pwmparams .6 0.01
# pid control for M0 using own position sensor as process variable
pid 0 0 5 s2    0.0027777778 1    5 0.0 0.2   3 0.01
# and M1's position sensor as setpoint
set var -999999999 999999999 1 0   1 0 5 s2    0.0027777778 1

# or with 2:1 gearing
set var -999999999 999999999 1 0   1 0 5 s2    0.0055555555 2

5. Motor 0 speed tracks position of motor 1

# set up port 1 motor to get all the variables
port 1 ; coast ; combi 0 1 0 2 0 3 0 ; select 0
port 0 ; combi 0 1 0 2 0 3 0 ; select 0 ; pwmparams .6 0.01
# pid control for M0 using derivative of own position sensor as process variable
pid_diff 0 0 5 s2    0.0027777778 1    0 2.5 0   .4 0.01
# and M1's position sensor as setpoint
set var -999999999 999999999 1 0   1 0 5 s2    0.02 0
*/

#include <string.h>
#include "common.h"
#include "debug.h"
#include "ioconv.h"
#include "timer.h"
#include "hardware.h"
#include "ports.h"
#include "control.h"
#include "device.h"
#include "pwm_pid.h"
#include "message.h"

// =============================== MAIN STATE MACHINE ==========================

// global timers
// gtimer 0: PORTFAULT reporting
// gtimer 1: MOTORFAULT reporting
#define NGTIMERS 2

// per-port timers
// timer 0: NACK timing during data phase
// timer 1: watchdog during setup and data phases
// timer 2: PID update
// timer 3: MODE data output
#define NTIMERS 4

// counter 0: signature bit generation
// counter 1: how many times we have seen this passive ID consecutively (up to 5)
#define NCOUNTERS 2

// report mode data
static void reportmode(int i) {
  if(portinfo[i].seloffset<0) device_dumpmodefmt(i,portinfo[i].selmode);
  else                        device_dumpmodevar(i,portinfo[i].selmode,portinfo[i].seloffset,portinfo[i].selformat);
  }

// the main loop
void go() {
  int i,j,u;
  int id;
  int state[NPORTS];                             // state of each port's state machine
  int delay[NPORTS];                             // time to next action
  int timers[NPORTS][NTIMERS];                   // timers for each port
  int gtimers[NGTIMERS];                         // global timers
  int counters[NPORTS][NCOUNTERS];               // counters for each port
  int sig[NPORTS];                               // signature of passive device on each port
  unsigned int t0,deltat;                        // global timing variables
  for(i=0;i<NPORTS;i++) {                        // initialise all the ports
    state[i]=0;
    delay[i]=0;
    device_init(i);
    port_uartoff(i);
    }
  memset(timers,0,sizeof(timers));               // initialise timers etc.
  memset(gtimers,0,sizeof(gtimers));
  memset(counters,0,sizeof(counters));
  t0=gettick();
  for(;;) {
    deltat=gettick()-t0;                         // milliseconds since we were last here
    if(deltat>2) {                               // normally this will be 0 or 1
      ostr("deltat="); o8hex(deltat); onl();     // emit a message if something has taken a relatively long time in the main loop
      }
    t0+=deltat;

// GLOBAL PROCESSING

    for(j=0;j<NGTIMERS;j++) gtimers[j]+=deltat;  // update global timers
    if(gpio_get(PIN_PORTFAULT)!=0) gtimers[0]=0;
    if(gtimers[0]>10&&gtimers[0]<1000) gtimers[0]=2000;  // detect port faults, but avoid 10ms glitches; trigger message immediately then at 1Hz
    if(gtimers[0]>=2000) {
      ostrnl("Port power fault");
      gtimers[0]-=1000;
      }
    if(gtimers[1]>=0) {                                   // detect motor faults, again avoiding glitches
      if(gpio_get(PIN_MOTORFAULT)!=0) gtimers[1]=0;       // hold at 0 while no fault
      if(gtimers[1]>10) {
        ostrnl("Motor power fault");
        port_checkmfaults();
        gtimers[1]=-1000;
        }
      }

// PER-PORT PROCESSING

    for(i=0;i<NPORTS;i++) {
      struct devinfo*d=devinfo+i;
      struct porthw*p=porthw+i;
      struct portinfo*q=portinfo+i;
      int pin_a=p->pin_tx,pin_b=p->pin_rx;         // appease compiler
      if(deltat) {                                 // non-zero time elapsed?
        delay[i]-=deltat;                          // count down any outstanding delay
        for(j=0;j<NTIMERS;j++) timers[i][j]+=deltat;       // update per-port timers
        }
      if(delay[i]>0) continue;                     // skip processing while we are delaying
      delay[i]=0;                                  // delay has expired, so reset it
      switch(state[i]) {
    case 0:                                        // state 0: disconnected
        d->id=0;
        if(d->connected) { o1ch('P'); o1hex(i); ostrnl(": disconnected"); }
        d->connected=0;
        port_initpwm(i);
        state[i]++;
        break;

/*
Passive device identification is done by configuring the RX and TX pins as outputs or
inputs (with pull-up or pull-down) in various combinations, and recording the values
seen on interface pins 5 and 6 in each case. Together these form a "signature" which
uniquely identifies the device type.
*/
    case 1:                                        // state 1: prepare to try to identify signature of passive device
        sig[i]=0;
        counters[i][0]=0;
        state[i]++;
        gpio_pull_down(p->pin_tx);                 // put TX and RX pins into a known state
        gpio_pull_down(p->pin_rx);
        delay[i]=10;                               // and allow to settle
        break;
    case 2:
        if(counters[i][0]<4) pin_a=p->pin_tx,pin_b=p->pin_rx;      // first try A=TX, B=RX, then try A=RX, B=TX
        else                 pin_a=p->pin_rx,pin_b=p->pin_tx;
        gpio_set_dir(pin_a,0);
        gpio_disable_pulls(pin_a);
        switch(counters[i][0]%4) {
      case 0: gpio_set_dir(pin_b,1); gpio_put(pin_b,0);     break; // output L
      case 1: gpio_set_dir(pin_b,0); gpio_pull_down(pin_b); break; // input, pull-down
      case 2: gpio_set_dir(pin_b,1); gpio_put(pin_b,1);     break; // output H
      case 3: gpio_set_dir(pin_b,0); gpio_pull_up(pin_b);   break; // input, pull-up
          }
        delay[i]=1;                               // so we cannot generate a valid RS-232-style character
        state[i]++;
        break;
    case 3:
        if(counters[i][0]<4) pin_a=p->pin_tx,pin_b=p->pin_rx;
        else                 pin_a=p->pin_rx,pin_b=p->pin_tx;
        u=port_state56(i);                       // read pins 5 and 6
        if(u&1) sig[i]|=0x001<<counters[i][0];   // and build up the signature
        if(u&2) sig[i]|=0x100<<counters[i][0];
        gpio_pull_down(pin_a);                   // set to defined level during settling time
        counters[i][0]++;
        if(counters[i][0]<8) state[i]--;         // loop between states 2 and 3 until we have collected the signature
        else                 state[i]++;
        delay[i]=10;                             // so we cannot generate a valid character
        break;
    case 4:
        gpio_set_dir(pin_a,0);                   // state 4: signature acquired
        gpio_set_dir(pin_b,0);
        d->signature=sig[i];
DEB_SIG         { o1ch('P'); o1hex(i); ostr(": D5 signature="); o4hex(sig[i]); }
        if((d->signature&0x00ef)==0x00cf) d->signature=0x00cf; // ignore random RX data from active ID
        switch(d->signature) {                 // convert signature to device type
      default:
      case 0x1cc0:                             // extra case to cover board build with RS-485 drivers not fitted
      case 0x0cc0: id= 0; break;               // nothing connected
      case 0x0040: id= 1; break;               // System medium motor
      case 0x00ff: id= 2; break;               // System train motor
      case 0xff00: id= 3; break;               // System turntable motor
      case 0x0000: id= 4; break;               // general PWM/third party
      case 0x4444:
      case 0x4c4c:
      case 0xc4c4:
      case 0xcccc: id= 5; break;               // button/touch sensor
      case 0xffff: id= 6; break;               // Technic large motor
      case 0xff40: id= 7; break;               // Technic XL motor (note that some have active ID!)
      case 0x1c40:                             // extra case to cover board build with RS-485 drivers not fitted
      case 0x0c40: id= 8; break;               // simple lights
      case 0x1cff:                             // extra case to cover board build with RS-485 drivers not fitted
      case 0x0cff: id= 9; break;               // Future lights 1
      case 0x1c00:                             // extra case to cover board build with RS-485 drivers not fitted
      case 0x0c00: id=10; break;               // Future lights 2
      case 0x04c0: id=11; break;               // System future actuator (train points)
      case 0x00cf: id=99; break;               // potentially an active ID
          }
DEB_SIG        { ostr(" id="); odec(id); onl(); }
        if(id==99) {
          state[i]=90;                             // attempt to decode active ID
          break;
          }
        if((d->id!=0&&id!=d->id) || id==0) {       // the ID is not constant: reset it
          d->id=0;
          counters[i][1]=0;
          state[i]=0;                              // reset everything
          break;
          }
        delay[i]=100;
        state[i]=1;                                // try to establish connection
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
          u=(d->signature!=0xcccc);
          if(d->buttonpressed!=u) {
            o1ch('P'); o1hex(i); ostrnl(u?": button pressed":": button released");
            }
          d->buttonpressed=u;
          }
        break;

// ============================== ACTIVE ID PROCESSING =========================

    case 90:                                       // here we may have an active ID to detect
        gpio_set_dir(p->pin_rx,0); gpio_pull_up(p->pin_rx);
        timers[i][1]=0;
        state[i]++;
        break;
    case 91:
        if(gpio_get(p->pin_rx)==1) {               // look to see RXD held low for 100ms
          state[i]=0;
          break;
          }
        if(timers[i][1]>=100) state[i]++;
        break;
    case 92:
        port_uarton(i);                            // enable UART on port for setup phase
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
            port_uartoff(i);
            state[i]=0;
            }
          if(q->checksumerrors>0) {
            o1ch('P'); o1hex(i); ostrnl(": checksum error: disconnecting");
            q->checksumerrors=0;
            port_uartoff(i);
            state[i]=0;
            }
          if(timers[i][1]>2000) {
            o1ch('P'); o1hex(i); ostrnl(": timeout during setup phase: disconnecting");
            port_uartoff(i);
            state[i]=0;
            }
          }
        else timers[i][1]=0;
        proc_ctrl();
        if(u!=0x04) break;                         // not an ACK: keep reading messages
        state[i]++;
        break;
    case 101:                                      // we have received an ACK
        if(d->type<0) {                            // no TYPE information?
          state[i]=0;                              // reset and try again
          break;
          }
        delay[i]=10;
        state[i]++;
        break;
    case 102:                                      // send the baud rate back to the device; pause
        device_sendint(i,0x42,-1,d->baud);
        delay[i]=100;
        state[i]++;
        break;
    case 103:
        device_sendsys(i,4);                       // send ACK; pause
        delay[i]=10;
        state[i]++;
        break;
    case 104:
        port_setbaud(i,d->baud);                   // switch baud rate
        ostr("set baud rate to "); odec(d->baud); onl();
        delay[i]=1;
        state[i]++;
        break;
    case 105:
        timers[i][0]=0;                            // reset communications timeouts
        timers[i][1]=0;
        timers[i][3]=0;
        portinfo[i].selmode=-1;
        portinfo[i].selreprate=-2;
        d->connected=1;
        device_dump(i);                            // dump device data and pause
        o1ch('P'); o1hex(i); ostr(": established serial communication with active ID "); o2hex(d->type); onl();
        state[i]=200;
        if(d->type==0x40) {                        // the LED matrix (id 0x40) requires power to be applied very quickly after connecting
          portinfo[i].spwaveshape =WAVE_SQUARE;    // as otherwise it disconnects; doing it here eases the timing requirements on
          portinfo[i].spwavemin   =1.0;            // the host side
          portinfo[i].spwavemax   =1.0;
          portinfo[i].spwaveperiod=1;
          portinfo[i].spwavephase =0;
          portinfo[i].coast=0;
          }
        break;

// ============================= ACTIVE ID: DATA PHASE =========================

    case 200:                                      // process data phase messages
        u=proc_datamessages(i);
        if(u==-1) {                                // idle?
          if(q->framingerrors+q->checksumerrors>5) {
            o1ch('P'); o1hex(i); ostr(": too many errors (");
            odec(q->framingerrors); o1ch('+');
            odec(q->checksumerrors); ostrnl("): disconnecting");
            port_uartoff(i);
            portinfo[i].selmode=-1;
            portinfo[i].selreprate=-2;
            state[i]=0;
            }
          if(timers[i][1]>500) {
            o1ch('P'); o1hex(i); ostrnl(": timeout during data phase: disconnecting");
            port_uartoff(i);
            portinfo[i].selmode=-1;
            portinfo[i].selreprate=-2;
            state[i]=0;
            }
          }
        else {
          timers[i][1]=0;                        // we have received a message from this device, so reset its watchdog
          }
        if(timers[i][0]>=100) {                  // make sure we send a NACK every 100ms in the absence of other communications
          timers[i][0]-=100;
          state[i]++;
          }
        if(portinfo[i].selmode<0) break;         // not listening for mode data?
        if(portinfo[i].selrxcount==0) {
          timers[i][3]=0;                        // hold timer reset until we receive at least one message
          break;
          }
        if(!ctrl_ospace()) break;                // if there is not plenty of room in the output buffer, do not output anything
        if(portinfo[i].selreprate==-2) break;    // disabled? don't report anything
        if(portinfo[i].selreprate==-1) {         // "once only" mode?
          reportmode(i);
          portinfo[i].selreprate=-2;             // disable further reports
          break;
          }
        if(portinfo[i].selreprate==0) {          // "direct" mode?
          reportmode(i);
          portinfo[i].selrxcount=0;
          break;
          }
        if(timers[i][3]>=portinfo[i].selreprate) {         // timed mode
          reportmode(i);
          timers[i][3]=0;
          break;
          }
        break;
    case 201:
        device_sendsys(i,2);                     // send NACK
        state[i]--;
        break;
        }                                        // end of state machine
      }                                          // end of port loop

    for(i=0;i<NPORTS;i++) {
      if(timers[i][2]>=PWM_UPDATE) {             // PID update
        timers[i][2]-=PWM_UPDATE;
        proc_pwm(i);
        }
      }

    proc_ctrl();                                 // process any incoming commands
    }
  }

// =================================== main() ==================================

int main() {
  init_timer();
  init_control();
  init_ports();
  init_accel();
  go();
  return 0;
  }
