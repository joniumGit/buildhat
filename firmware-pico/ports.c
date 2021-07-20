#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "debug.h"
#include "control.h"
#include "ioconv.h"
#include "timer.h"
#include "hardware.h"
#include "ports.h"

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/resets.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/pio.h"

#include "uart_tx.pio.h"
#include "uart_rx.pio.h"

static UC driverdata[NPORTS][DRIVERBYTES];

struct portinfo portinfo[NPORTS];
struct message messages[NPORTS];
volatile struct message mqueue[NPORTS][MQLEN];
volatile int mqhead[NPORTS],mqtail[NPORTS];        // if head==tail then the queue is empty

static int port_i2c_write(int p,const uint8_t*src,size_t len,bool nostop) {
  int u;
  u=i2c_write_timeout_us(porthw[p].i2c,porthw[p].i2c_add,src,len,nostop,10000); // allow 10ms timeout
  if(u==PICO_ERROR_GENERIC) { ostr("<P"); odec(p); ostrnl(": generic I²C bus error>"); }
  if(u==PICO_ERROR_TIMEOUT) { ostr("<P"); odec(p); ostrnl(": I²C bus timeout>"      ); }
  return u;
  }

static int port_i2c_read(int p,uint8_t*dst,size_t len,bool nostop) {
  int u;
  u=i2c_read_timeout_us (porthw[p].i2c,porthw[p].i2c_add,dst,len,nostop,10000); // allow 10ms timeout
  return u;
  }

static int port_readi2cbyte(int p,int b) {
  UC t;
  t=b;
  if(port_i2c_write(p,&t,1,0)==-2) return -1;
  if(port_i2c_read (p,&t,1,0)==-2) return -1;
  return t;
  }

static inline void port_setreg(int p,int r,int d) {
  UC t[2]={r,d};
  port_i2c_write(p,t,2,0);
  }

static inline void port_set2reg(int p,int r,int d0,int d1) {
  UC t[3]={r,d0,d1};
  port_i2c_write(p,t,3,0);
  }

static inline void port_set_pwmflags(int p,int f) {
  port_setreg(p,0x4C,f);
  }

static inline void port_set_pwmamount(int p,int a) {
  port_set2reg(p,0xA1,a,0x01);  // set PWM0 initial duty cycle value [7:0]; set I2C trigger for PWM0 to Update duty cycle value
  }

// set PWM values as integer
static void port_set_pwm_int(int pn,int pwm) {
  struct portinfo*q=portinfo+pn;
  int lpwm=q->lastpwm;
  if(pwm==lpwm) return;
  q->lastpwm=pwm;
  if(pwm==0) {
    port_set_pwmflags(pn,0x6f); // disable PWM
    port_set_pwmamount(pn,1); // minimum amount
    return;
    }
  if(ABS(lpwm)>ABS(pwm)) port_set_pwmamount(pn,ABS(pwm)); // amount reducing in absolute terms? then set it first
  if(pwm< 0&&lpwm>=0) port_set_pwmflags(pn,0x5f); // enable, -ve direction
  if(pwm>=0&&lpwm<=0) port_set_pwmflags(pn,0x7f); // enable, +ve direction
  if(ABS(lpwm)<=ABS(pwm)) port_set_pwmamount(pn,ABS(pwm)); // amount increasing in absolute terms? then set it last
  }

// set PWM values according to pwm:
// -1 full power reverse
// +1 full power forwards
void port_set_pwm(int p,float pwm) {
  int u;
  CLAMP(pwm,-1,1);
  u=(int)(pwm*PWM_PERIOD+0.5);
  port_set_pwm_int(p,u);
  }

void port_motor_brake(int pn) {
//!!!
  }

void port_initpwm() {
  int i;
  for(i=0;i<NPORTS;i++) {
    portinfo[i].lastpwm=0x7fffffff; // dummy value
    port_set_pwm_int(i,0);
    }
  }

// GPIO5 -> b0, GPIO6 -> b1
unsigned int port_state56(int p) {
  unsigned int u;
  u=port_readi2cbyte(p,0x4b);
  u=(u>>6)&3;
  return u;
  }

void port_resetdriver(int p) {
  UC t[2]={0xf5,0x01};
  port_i2c_write(p,t,2,0);
  }

void port_initdriver(int p) {
  port_setreg(p,0x6A,0x00);
  port_setreg(p,0x6B,0x00); // disable pull-ups/downs on GPIO5/6
//  port_setreg(p,0x67,0x6A);
//  port_setreg(p,0x68,0x15); // enable 10kΩ pull-ups and Schmitt triggers on SCL, SDA !!!
  port_setreg(p,0x5C,0x20); // set PWM0 Period CLK to OSC1 Flex-Div
  port_setreg(p,0x5D,0x03); // set OSC1 Flex-Div to 4
  port_setreg(p,0x9D,0x0E); // set 2-bit LUT2 Logic to OR
  port_setreg(p,0x19,0x7A); // connection 2-bit LUT2 IN1 to I2C OUT7  Connection 2-bit LUT2 IN0 to PWM0 OUT+
  port_setreg(p,0x1A,0x06); //
  port_setreg(p,0x09,0x03); // connection HV OUT CTRL0 EN Input to 2-bit LUT2 OUT
  port_setreg(p,0x0C,0x03); // connection HV OUT CTRL1 EN Input to 2-bit LUT2 OUT
  port_setreg(p,0x3C,0x16); // connection PWM0 PWR DOWN Input to 3-bit LUT9 OUT
  port_setreg(p,0xB9,0x09); // set PWM0 to flex-Div clock
  port_setreg(p,0x4c,0x70); // clear fault flags
  port_setreg(p,0x4C,0x7F); // enable fault flags
  }

//void port_setpin6(int p,int s) {
//  UC t[3]={0x07,driverdata[pn][0x07],driverdata[pn][0x08]};
//  t[2]=0; t[1]&=0x0f;
//  switch(s) {
//case 0: t[2]|=0xfc; t[1]|=0x00; break;
//case 1: t[2]|=0x00; t[1]|=0x00; break;
//case 2: t[2]|=0xff; t[1]|=0xf0; break;
//    }
//  port_i2c_write(p,t,3,0);
//  }

static void port_uart_irq(int pn);

static void port0_uart_irq() { port_uart_irq(0); }
static void port1_uart_irq() { port_uart_irq(1); }
static void port2_uart_irq() { port_uart_irq(2); }
static void port3_uart_irq() { port_uart_irq(3); }

static int txprogoffset0,rxprogoffset0;
static int txprogoffset1,rxprogoffset1;

void init_ports() {
  int i,j;
  struct porthw*p;
  reset_block(RESETS_RESET_PIO0_BITS);
  reset_block(RESETS_RESET_PIO1_BITS);
  unreset_block_wait(RESETS_RESET_PIO0_BITS);
  unreset_block_wait(RESETS_RESET_PIO1_BITS);

  ostrnl("Initialising ports");
  memset(portinfo,0,sizeof(portinfo));
  gpio_init(PIN_LED0      ); gpio_set_dir(PIN_LED0      ,1);
  gpio_init(PIN_LED1      ); gpio_set_dir(PIN_LED1      ,1);
  gpio_init(PIN_PORTFAULT ); gpio_set_dir(PIN_PORTFAULT ,0); gpio_pull_up(PIN_PORTFAULT);
  gpio_init(PIN_PORTON    ); gpio_set_dir(PIN_PORTON    ,1); gpio_put(PIN_PORTON,0);
  gpio_init(PIN_MOTORFAULT); gpio_set_dir(PIN_MOTORFAULT,0); gpio_pull_up(PIN_MOTORFAULT);
  i2c_init(i2c0,400000);
  i2c_init(i2c1,400000);
  gpio_set_function(PIN_I2C0_SDA,GPIO_FUNC_I2C);
  gpio_set_function(PIN_I2C0_SCL,GPIO_FUNC_I2C);
  gpio_set_function(PIN_I2C1_SDA,GPIO_FUNC_I2C);
  gpio_set_function(PIN_I2C1_SCL,GPIO_FUNC_I2C);
  gpio_pull_up(PIN_I2C0_SDA);
  gpio_pull_up(PIN_I2C0_SCL);
  gpio_pull_up(PIN_I2C1_SDA);
  gpio_pull_up(PIN_I2C1_SCL);
  for(i=0;i<NPORTS;i++) {
    mqhead[i]=mqtail[i]=0;
    p=porthw+i;
    gpio_init(p->pin_rts); gpio_set_dir(p->pin_rts,1); gpio_put(p->pin_rts,0);
    gpio_init(p->pin_dio); gpio_set_dir(p->pin_dio,0);
    gpio_init(p->pin_rx ); gpio_set_dir(p->pin_rx ,0);
    gpio_init(p->pin_tx ); gpio_set_dir(p->pin_tx ,0);
//    padsbank0_hw->io[p->pin_rx]&=~0x30; // drive strength 2mA
//    padsbank0_hw->io[p->pin_tx]&=~0x30;
    portinfo[i].selmode=-1;
    }
  gpio_put(PIN_PORTON,1);                          // enable port power
#ifdef DEBUG_PINS
  gpio_init(PIN_DEBUG0); gpio_set_dir(PIN_DEBUG0,1);
  gpio_init(PIN_DEBUG1); gpio_set_dir(PIN_DEBUG1,1);
#endif

  wait_ticks(100);
//  ostrnl("Checking I²C0:");
//  for(i=0x08;i<0x80;i++) {
//    unsigned char t;
//    if(i%8==0) { o2hex(i); osp(); }
//    if(i2c_read_blocking(i2c0,i,&t,1,0)==-2) o1ch('.'); else o1ch('+'); osp();
//    if(i%8==7) onl();
//    }
//  ostrnl("Checking I²C1:");
//  for(i=0x08;i<0x80;i++) {
//    if(i%8==0) { o2hex(i); osp(); }
//    unsigned char t;
//    if(i2c_read_blocking(i2c1,i,&t,1,0)==-2) o1ch('.'); else o1ch('+'); osp();
//    if(i%8==7) onl();
//    }

  ostr("Resetting drivers: ");
  for(i=0;i<NPORTS;i++) {
    odec(i);
    port_resetdriver(i);
    }
  onl();
  wait_ticks(100);

  ostr("Reading driver dumps: before port_initdriver()");
  for(i=0;i<NPORTS;i++) {
    odec(i);
    for(j=0;j<DRIVERBYTES;j++) {
      driverdata[i][j]=port_readi2cbyte(i,j);
      if(i==0) {
        o2hex(driverdata[i][j]);
        if(j%16==15) onl();
        else         osp();
        }
      }
    }
  onl();

  for(i=0;i<NPORTS;i++) port_initdriver(i);

  ostr("Reading driver dumps: after port_initdriver()");
  for(i=0;i<NPORTS;i++) {
    odec(i);
    for(j=0;j<DRIVERBYTES;j++) {
      driverdata[i][j]=port_readi2cbyte(i,j);
      if(i==0) {
        o2hex(driverdata[i][j]);
        if(j%16==15) onl();
        else         osp();
        }
      }
    }
  onl();

  pio_clear_instruction_memory(pio0);
  pio_clear_instruction_memory(pio1);

  txprogoffset0=pio_add_program(pio0,&uart_tx_program);
  rxprogoffset0=pio_add_program(pio0,&uart_rx_program);
  txprogoffset1=pio_add_program(pio1,&uart_tx_program);
  rxprogoffset1=pio_add_program(pio1,&uart_rx_program);
  if(txprogoffset0!=txprogoffset1 ||
     rxprogoffset0!=rxprogoffset1) {
     ostrnl("Assertion failed");
     exit(16);
     }
//  o2hex(txprogoffset0); osp(); o2hex(rxprogoffset0); onl();
//  o2hex(txprogoffset1); osp(); o2hex(rxprogoffset1); onl();

  irq_set_exclusive_handler(PIO0_IRQ_0,port0_uart_irq);
  irq_set_exclusive_handler(PIO0_IRQ_1,port1_uart_irq);
  irq_set_exclusive_handler(PIO1_IRQ_0,port2_uart_irq);
  irq_set_exclusive_handler(PIO1_IRQ_1,port3_uart_irq);

  port_initpwm();
//  // test for PWM glitches
//  for(;;) {
//    for(i=2;i<4;i++) {
//      port_set_pwm_int(1,i);
//      wait_ticks(2);
//      o1ch('.');
//      }
//    }


//  for(j=0;j<20;j++) {
//    struct porthw*p=porthw+1;
//    int s=0,u;
//
//    for(i=0;i<4;i++) {
//      gpio_put(PIN_DEBUG0,i==0);
//      gpio_set_dir(p->pin_tx,0);
//      gpio_disable_pulls(p->pin_tx);
//      switch(i) {
//    case 0: gpio_set_dir(p->pin_rx,1); gpio_put(p->pin_rx,0);     break;
//    case 1: gpio_set_dir(p->pin_rx,0); gpio_pull_down(p->pin_rx); break;
//    case 2: gpio_set_dir(p->pin_rx,0); gpio_pull_up(p->pin_rx);   break;
//    case 3: gpio_set_dir(p->pin_rx,1); gpio_put(p->pin_rx,1);     break;
//        }
//      wait_ticks(10);
//      u=port_state56(1);
//      if(u&1) s|=0x0001<<i;
//      if(u&2) s|=0x0100<<i;
//      }
//
//    for(i=0;i<4;i++) {
//      gpio_set_dir(p->pin_rx,0);
//      gpio_disable_pulls(p->pin_rx);
//      switch(i) {
//    case 0: gpio_set_dir(p->pin_tx,1); gpio_put(p->pin_tx,0);     break;
//    case 1: gpio_set_dir(p->pin_tx,0); gpio_pull_down(p->pin_tx); break;
//    case 2: gpio_set_dir(p->pin_tx,0); gpio_pull_up(p->pin_tx);   break;
//    case 3: gpio_set_dir(p->pin_tx,1); gpio_put(p->pin_tx,1);     break;
//        }
//      wait_ticks(10);
//      u=port_state56(1);
//      if(u&1) s|=0x0010<<i;
//      if(u&2) s|=0x1000<<i;
//      }
//
//    o4hex(s); onl();
//    if(j==10) {
//      port_uarton(1);
//      port_uartoff(1);
//      onl();
//      }
//    }

  ostrnl("Done initialising ports");
  }

// ========================== LPF2 port UARTs and ISRs =========================

void port_setbaud(int pn,int baud) {
  struct porthw*p=porthw+pn;
  if(baud<  2400) baud=  2400;
  if(baud>115200) baud=115200;
  float div=(float)clock_get_hz(clk_sys)/(8*baud)*65536.0;
  p->pio->sm[p->txsm].clkdiv=((int)div)&~0xff;
  p->pio->sm[p->rxsm].clkdiv=((int)div)&~0xff;
  }

// message state machine
#define MS_NOSYNC  -3
#define MS_MTYPE   -2
#define MS_CMD     -1
#define MS_PAYLOAD  0

void port_uarton(int pn) {
  struct porthw*p=porthw+pn;
  struct portinfo*q=portinfo+pn;
  PIO pio=p->pio;
  int tsm=p->txsm;
  int rsm=p->rxsm;
  int txp=p->pin_tx;
  int rxp=p->pin_rx;
  pio_sm_config c;
  float div=(float)clock_get_hz(clk_sys)/(8*2400);   // SMs transmit 1 bit per 8 execution cycles; dummy baud rate

// TX init
  pio_sm_set_pins_with_mask(pio,tsm,1<<txp,1<<txp);    // tell PIO to initially drive output-high on the selected pin, then map PIO
  pio_sm_set_pindirs_with_mask(pio,tsm,1<<txp,1<<txp); // onto that pin with the IO muxes
  pio_gpio_init(pio,txp);
  c=uart_tx_program_get_default_config(txprogoffset0);
  sm_config_set_out_shift(&c,true,false,32);          // OUT shifts to right, no autopull

  sm_config_set_out_pins(&c, txp, 1);   // we map both OUT and side-set to the same pin, because sometimes we need to assert
  sm_config_set_sideset_pins(&c, txp);  // user data onto the pin (with OUT) and sometimes constant values (start/stop bit)
  sm_config_set_clkdiv(&c,div);

  pio_sm_init(pio,tsm,txprogoffset0,&c);
  pio_sm_set_enabled(pio,tsm,true);

// RX init
  pio_sm_set_consecutive_pindirs(pio,rsm,rxp,1,false);
  pio_gpio_init(pio,rxp);
  gpio_pull_up(rxp);

  c=uart_rx_program_get_default_config(rxprogoffset0);
  sm_config_set_in_pins(&c,rxp); // for WAIT, IN
  sm_config_set_jmp_pin(&c,rxp); // for JMP
  sm_config_set_in_shift(&c, true, false, 32); // shift to right, autopull disabled
  sm_config_set_clkdiv(&c,div);

  q->mstate=MS_NOSYNC;
  q->lasttick=gettick();
  q->framingerrors=0;
  q->checksumerrors=0;
  q->txptr=-1;
  q->txlen=0;

  pio_sm_init(pio,rsm,rxprogoffset0,&c);
  switch(pn) { //!!! this is not very elegant
case 0:pio0->inte0=0x12; break;
case 1:pio0->inte1=0x48; break;
case 2:pio1->inte0=0x12; break;
case 3:pio1->inte1=0x48; break;
    }
  pio_sm_set_enabled(pio,rsm,true);
  irq_set_enabled(p->irq,1);
  }

void port_uartoff(int pn) {
  struct porthw*p=porthw+pn;
  struct portinfo*q=portinfo+pn;
  PIO pio=p->pio;
  int tsm=p->txsm;
  int rsm=p->rxsm;
  int txp=p->pin_tx;
  int rxp=p->pin_rx;

  pio_sm_set_enabled(pio,tsm,false);
  pio_sm_set_enabled(pio,rsm,false);
  gpio_init(txp); gpio_set_dir(txp,0);
  gpio_init(rxp); gpio_set_dir(rxp,0);
  irq_set_enabled(p->irq,0);
  switch(pn) { //!!! this is not very elegant
case 0:pio0->inte0=0; break;
case 1:pio0->inte1=0; break;
case 2:pio1->inte0=0; break;
case 3:pio1->inte1=0; break;
    }
  q->mstate=MS_NOSYNC;
  q->txptr=-2;
  }

int port_waitch(int pn) {
  struct porthw*p=porthw+pn;
  PIO pio=p->pio;
  int rsm=p->rxsm;
  while(pio_sm_is_rx_fifo_empty(pio,rsm)) ;
  return (int)*((io_rw_8*)&pio->rxf[rsm]+3);
  }

static void port_uart_irq(int pn) {
  struct porthw*p=porthw+pn;
  struct portinfo*q=portinfo+pn;
  PIO pio=p->pio;
  int tsm=p->txsm;
  int rsm=p->rxsm;
  struct message*m=messages+pn;
  int b,i,f0,f1,f2;

  if(!pio_sm_is_tx_fifo_full(pio,tsm)) {
    if(q->txptr>=0) {
//      gpio_put(PIN_DEBUG0,1);
      pio_sm_put_blocking(pio,tsm,q->txbuf[q->txptr++]); // send next character
//      gpio_put(PIN_DEBUG0,0);
      if(q->txptr==q->txlen) {                     // finished?
        q->txptr=-1;
        *p->inte=p->intb&PORT_INTE_RXMASK;     // only enable receive interrupt now
        }
      }
    else
      *p->inte=p->intb&PORT_INTE_RXMASK;     // only enable receive interrupt now
    }
//!!!  if(s&0x02) {                                     // framing error? this detects break condition
//!!!    q->framingerrors++;
//!!!DEB_SER    ostrnl("*FrE*");
//!!!    b=u->DR;                                       // clear framing error
//!!!    q->mstate=MS_MTYPE;
//!!!    return;
//!!!    }
  if(!pio_sm_is_rx_fifo_empty(pio,rsm)) {         // RX not empty?
    b=(int)*((io_rw_8*)&pio->rxf[rsm]+3);           // get byte
    m->check^=b;                                   // accumulate checksum
  DEB_SER  o2hex(b);
    switch(q->mstate) {
  case MS_NOSYNC:
  DEB_SER    o1ch('n');
      if(gettick()-q->lasttick<100) break;              // wait for a pause
      q->mstate=MS_MTYPE;
      // and fall through
  case MS_MTYPE:
  DEB_SER    o1ch('t');
      m->check=b;                                  // initialise checksum
      f0=(b>>6)&3;                                 // split byte into three fields
      f1=(b>>3)&7;
      f2=(b>>0)&7;
      m->type=b&0xc7;
      m->cmd=0;
      m->mode=0;
      m->plen=0;
      switch(f0) {                                 // examine top two bits
    case 0:                                        // "system message"
        switch(f2) {
      case 0:
      case 1:
      case 2: // NACK
      case 3:
      case 6:
      case 7:
          break;                                   // SYNC-type messages ignored
      case 4: // ACK
  DEB_SER        ostr("ACK");
          m->check=0xff;
          goto complete;
      case 5: // PRG
          m->plen=1<<f1;
          q->mstate=MS_CMD;
          break;
          }
        break;
    case 1:                                        // "command message"
        m->plen=1<<f1;
        q->mstate=MS_PAYLOAD;                      // no extra command byte in this case
        break;
    case 2:                                        // "info message"
        m->mode=m->type&7;                         // move mode bits to "mode"
        m->type&=~7;
        m->plen=1<<f1;
        q->mstate=MS_CMD;                          // command byte comes next
        break;
    case 3:                                        // "data message"
        m->mode=m->type&7;                         // move mode bits to "mode"
        m->type&=~7;
        m->plen=1<<f1;
        q->mstate=MS_PAYLOAD;                      // payload comes next
        break;
        }
      break;
  case MS_CMD:
  DEB_SER    o1ch('c');
      m->cmd=b;
      m->mode|=(b>>2)&0x08;                        // extended mode bit
      q->mstate=MS_PAYLOAD;
      break;
  default:
  DEB_SER    o1ch('p');
      if(q->mstate<(int)m->plen) {                 // mstate goes from 0 to plen-1 as payload bytes are read
        m->payload[q->mstate++]=b;
        break;
        }
  complete:
      // here we have a complete message with checksum incorporated into m->check
      if(m->check!=0xff) q->checksumerrors++;
      else {                                       // if checksum OK add to message queue
        i=(mqhead[pn]+1)%MQLEN;
        if(i!=mqtail[pn]) {                        // discard on overrunning the message queue
          memcpy((void*)&mqueue[pn][mqhead[pn]],m,sizeof(struct message));
          mqhead[pn]=i;
        } else {
          ostrnl("MQ overrun!");                   // !!! consider trapping this
          }
        }
      q->mstate=MS_MTYPE;                          // ready for next message
      break;
      }
    q->lasttick=gettick();
//    gpio_put(PIN_DEBUG0,0);
    }
  }

void port_sendmessage(int pn,unsigned char*buf,int l) {
  struct porthw*p=porthw+pn;
  struct portinfo*q=portinfo+pn;
  if(l<1) return;
  if(l>TXBLEN) l=TXBLEN;
  if(q->txptr==-2) return;                         // UART off? ignore
  while(q->txptr>=0) ;                             // wait for TX idle
  memcpy((void*)q->txbuf,buf,l);
  q->txptr=0;
  q->txlen=l;
  *p->inte=p->intb;  // both interrupts enabled
  switch(pn) { //!!! this is not very elegant
case 0:pio0->inte0=0x12; break;
case 1:pio0->inte1=0x48; break;
case 2:pio1->inte0=0x12; break;
case 3:pio1->inte1=0x48; break;
    }
  }

