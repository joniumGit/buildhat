#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "debug.h"
#include "control.h"
#include "ioconv.h"
#include "timer.h"
#include "hardware.h"
#include "ports.h"
#include "pwm_pid.h"

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

// attempt to reset the I²C bus: see Analog Devices application note AN-686
static void i2c_reset(i2c_inst_t*i2c) {
  int i,scl,sda;
  ostrnl("Attempting reset of I²C bus");
  i2c_deinit(i2c);
  sda=i2c==i2c0?PIN_I2C0_SDA:PIN_I2C1_SDA;
  scl=i2c==i2c0?PIN_I2C0_SCL:PIN_I2C1_SCL;
  gpio_init(sda); gpio_pull_up(sda);
  gpio_init(scl); gpio_set_dir(scl,1); gpio_put(scl,1);    // force clock high
  for(i=0;i<9;i++) {
    wait_ticks(2);
    gpio_put(scl,0);                                       // generate a pulse on SCL
    wait_ticks(2);
    gpio_put(scl,1);
    }
  gpio_set_dir(sda,1); gpio_put(sda,1);                    // SDA high
  wait_ticks(2);
  gpio_put(scl,0);                                         // generate a rising edge on SCL with SDA high = STOP condition
  wait_ticks(2);
  gpio_put(scl,1);
  i2c_init(i2c,400000);                                    // reset interface
  gpio_init(sda); gpio_set_function(sda,GPIO_FUNC_I2C); gpio_pull_up(sda);
  gpio_init(scl); gpio_set_function(scl,GPIO_FUNC_I2C); gpio_pull_up(scl);
  wait_ticks(10);                                          // delay before retry
  }

// wrapper for SDK function incorporating retries and reset
static int i2c_write(i2c_inst_t*i2c,int add,const uint8_t*src,size_t len,bool nostop) {
  int i,j,u;
  for(j=0;;j++) {                                           // three tries, then attempt a reset, then one more try
    for(i=0;i<3;i++) {                                      // up to 3 tries the first time
      u=i2c_write_timeout_us(i2c,add,src,len,nostop,10000); // allow 10ms timeout
      if(u==len) return u;
      if(u==PICO_ERROR_GENERIC) { ostr("<generic I²C bus error>"); }
      if(u==PICO_ERROR_TIMEOUT) { ostr("<I²C bus timeout>"      ); }
      if(j) return u;                                       // give up and return an error code if we have already tried resetting
      wait_ticks(10);                                       // delay before retry
      }
    i2c_reset(i2c);
    }
  }

// wrapper for SDK function incorporating retries and reset
static int i2c_read(i2c_inst_t*i2c,int add,uint8_t*dst,size_t len,bool nostop) {
  int i,j,u;
  for(j=0;;j++) {                                           // three tries, then attempt a reset, then one more try
    for(i=0;i<3;i++) {                                      // up to 3 tries the first time
      u=i2c_read_timeout_us (i2c,add,dst,len,nostop,10000); // allow 10ms timeout
      if(u==len) return u;
      if(u==PICO_ERROR_GENERIC) { ostr("<generic I²C bus error>"); }
      if(u==PICO_ERROR_TIMEOUT) { ostr("<I²C bus timeout>"      ); }
      if(j) return u;                                       // give up and return an error code if we have already tried resetting
      wait_ticks(10);                                       // delay before retry
      }
    i2c_reset(i2c);
    }
  }

// read a byte from register b on port p
static int port_readi2cbyte(int p,int b) {
  UC t;
  t=b;
  if(i2c_write(porthw[p].i2c,porthw[p].i2c_add,&t,1,0)==-2) return -1;
  if(i2c_read (porthw[p].i2c,porthw[p].i2c_add,&t,1,0)==-2) return -1;
  return t;
  }

// write a byte d to register r on port p
static inline void port_setreg(int p,int r,int d) {
  UC t[2]={r,d};
  i2c_write(porthw[p].i2c,porthw[p].i2c_add,t,2,0);
  }

// write two bytes d0, d1 to register r, r+1 on port p
static inline void port_set2reg(int p,int r,int d0,int d1) {
  UC t[3]={r,d0,d1};
  i2c_write(porthw[p].i2c,porthw[p].i2c_add,t,3,0);
  }

// dump all registers of driver IC on port pn
void port_driverdump(int pn) {
  int j;
  for(j=0;j<DRIVERBYTES;j++) {
    o2hex(port_readi2cbyte(pn,j));
    if(j%16==15) onl();
    else         osp();
    }
  onl();
  }

// ====================================== ACCELEROMETER =================================

static void accel_setreg(int r,int d) {
  UC t[2]={r,d};
  i2c_write_timeout_us(ACCEL_I2C,ACCEL_I2C_ADD,t,2,0,10000); // allow 10ms timeout
  }

void init_accel() {
  accel_setreg(0x10,0x60);                       // enable accelerometer
  accel_setreg(0x11,0x60);                       // enable gyroscope
  accel_setreg(0x12,0x34);                       // make IRQ pin open drain
  }

// ======================================= LPF2 PORTS ===================================

static inline void port_set_pwmflags(int p,int f) {
  port_setreg(p,0x4C,f);                         // set flag bits in PWM driver
  }

static inline void port_set_pwmamount(int p,int a) {
  port_set2reg(p,0xA1,a,0x01);                   // set PWM0 initial duty cycle value [7:0]; set I2C trigger for PWM0 to Update duty cycle value
  }

// set PWM values as integer
static void port_set_pwm_int(int pn,int pwm) {
  struct portinfo*q=portinfo+pn;
  int lpwm=q->lastpwm;
  if(pwm==lpwm) return;
  q->lastpwm=pwm;
  if(ABS(lpwm)>ABS(pwm)) port_set_pwmamount(pn,ABS(pwm));                      // amount decreasing in absolute terms? then set it first
  if(pwm< 0&&(lpwm>=0||lpwm==0x7fffffff)) port_set_pwmflags(pn,0x5f);          // enable, -ve direction
  if(pwm>=0&&(lpwm<=0||lpwm==0x7fffffff)) port_set_pwmflags(pn,0x7f);          // enable, +ve direction
  if(ABS(lpwm)<=ABS(pwm)) port_set_pwmamount(pn,ABS(pwm));                     // amount increasing in absolute terms? then set it last
  }

// set PWM values according to pwm:
// -1 full power reverse
// +1 full power forwards
// applying pwmthresh mapping and then power limit
// called for each port (unless coasting) at a rate determined by PWM_UPDATE
void port_set_pwm(int p,float pwm) {
  int u,v;
  CLAMP(pwm,-1,1);
  u=((int)(pwm*131072)+1)/2;                // convert to rounded Q16
  if(ABS(u)<portinfo[p].minpwm) u=0;        // don't try to generate tiny PWM values
  if(ABS(u)>=portinfo[p].pwmthresh) v=u;    // if above slow/fast PWM switchover threshold, send value directly to PWM driver
  else {
    portinfo[p].pwmthreshacc+=u;            // enable PWM drivers at the threshold level at the correct average rate ("slow PWM")
    if     (portinfo[p].pwmthreshacc>= portinfo[p].pwmthresh) v= portinfo[p].pwmthresh;
    else if(portinfo[p].pwmthreshacc<=-portinfo[p].pwmthresh) v=-portinfo[p].pwmthresh;
    else                                                      v=0;
    portinfo[p].pwmthreshacc-=v;
    }
  CLAMP(v,-portinfo[p].pwm_drive_limit,portinfo[p].pwm_drive_limit);
  v=(v*PWM_PERIOD+PWM_PERIOD/2)>>16;        // map to ±PWM_PERIOD
  port_set_pwm_int(p,v);
  }

// set motor to coast
void port_motor_coast(int pn) {
  port_set_pwmflags(pn,0x6f);                    // disable PWM
  port_set_pwmamount(pn,1);                      // minimum amount
  portinfo[pn].lastpwm=0x7fffffff;
  }

// initialise scaled variable
static void initsvar(struct svar*sv) {
  sv->port=0;
  sv->mode=0;
  sv->offset=0;
  sv->format=0;
  sv->scale=0;
  sv->unwrap=0;
  sv->last=1.1e38;                               // values overr 1e38 flag that there is no valid "last" reading
  }

// initialise all PWM variables
void port_initpwm(int pn) {
  struct portinfo*q=portinfo+pn;
  port_set_pwm_int(pn,0);
  q->pwmmode=0;
  q->pwm_drive_limit=6554;                       // 0.1 Q16
  q->coast=0;
  q->lastpwm=0x7fffffff;                         // dummy value
  q->pwmthresh=0;
  q->pwmthreshacc=0;
  q->minpwm=0;
  q->setpoint=0;
  q->spwaveshape=WAVE_SQUARE;
  q->spwavemin=0;
  q->spwavemax=0;
  q->spwaveperiod=1;
  q->spwavephase=0;
  q->spwavephaseacc=0;
  q->pid_pv=0;
  q->pid_ierr=0;
  q->pid_perr=0;
  initsvar(&q->pvsvar);
  initsvar(&q->spsvar);
  q->Kp=0;
  q->Ki=0;
  q->Kd=0;
  q->windup=0;
  q->deadzone=0;
  }

// get state of GPIO5 in bit 0, GPIO6 in bit 1
unsigned int port_state56(int p) {
  unsigned int u;
  u=port_readi2cbyte(p,0x4b);
  u=(u>>6)&3;
  return u;
  }

// reset driver IC
void port_resetdriver(int p) {
  port_setreg(p,0xf5,0x01);
  }

// clear all fault flags
void port_clearfaults() {
  int i;
  for(i=0;i<NPORTS;i++) {
    port_setreg(i,0x4C,0x70);                    // clear fault flags
    port_setreg(i,0x4C,0x7F);                    // enable fault flags
    }
  }

// check for motor faults
void port_checkmfaults() {
  int i,p;
  UC t[NPORTS*3];
  UC r;
  r=0x4d;
  for(p=0;p<NPORTS;p++) {
    if(i2c_write(porthw[p].i2c,porthw[p].i2c_add,&r   ,1,0)==-2) goto err;
    if(i2c_read (porthw[p].i2c,porthw[p].i2c_add,t+p*3,3,0)==-2) goto err;
    }
  ostrnl(" Port 4D 4E 4F");                      // dump all the fault flags
  for(p=0;p<NPORTS;p++) {
    ostr("   "); odec(p); ostr(" ");
    for(i=0;i<3;i++) {
      osp();
      o2hex(t[p*3+i]);
      }
    onl();
    }
  return;
err:
  ostrnl("Error reading fault flags");
  }

void port_initdriver(int p) {
  port_setreg(p,0x6A,0x00);
  port_setreg(p,0x6B,0x00);            // disable pull-ups/downs on GPIO5/6
  port_setreg(p,0x97,0x01);            // fault signal
  port_setreg(p,0x98,0x00);            // fault signal
  port_setreg(p,0x5C,0x20);            // set PWM0 Period CLK to OSC1 Flex-Div
  port_setreg(p,0x5D,0x03);            // set OSC1 Flex-Div to 4
  port_setreg(p,0x9D,0x0E);            // set 2-bit LUT2 Logic to OR
  port_setreg(p,0x18,0x3C);            // bits 5..0 Matrix OUT 33 IN0 LUT2_2 OCP FAULT) 7..6 below 
  port_setreg(p,0x19,0x7A);            // connection 2-bit LUT2 IN1 to I2C OUT7  Connection 2-bit LUT2 IN0 to PWM0 OUT+
  port_setreg(p,0x1A,0x06);            // 7..2 Matrix out 33 DFF3 clk to LUT2_0 out  1:0 above
  
  port_setreg(p,0x92,0xc5);            // LUT2_3 values
  port_setreg(p,0x15,0xF2);            // 5..0 Matrix OUT28 IN1 LUT2_0 to Fault_A , 7..6 below
  port_setreg(p,0x16,0x0B);            // 3..0 Matrix OUT29 IN0 LUT2_3 to ACMP1H , 7..4 next byte ( GND)
  port_setreg(p,0x21,0x44);            // 7..2 Matrix out 33 DFF3 clk to LUT2_0 out  1:0 above
  
  port_setreg(p,0x2A,0xBE);            // make fault pin sourced from live inputs instead of latched inputs
  port_setreg(p,0x2B,0x10);            // 
  port_setreg(p,0x2C,0x10);            // 
  
  port_setreg(p,0x42,0xC0);            // pulse delay input from 2bit LUT2 OUT
  port_setreg(p,0x9A,0x20);            // rising edge, 486ns pulse
  
  port_setreg(p,0x6D,0x1E);            // enable OCP blanking for HV OUT 1
  port_setreg(p,0x09,0x03);            // connection HV OUT CTRL0 EN Input to 2-bit LUT2 OUT
  port_setreg(p,0x0C,0x0F);            // connection HV OUT CTRL1 EN Input via pulse delay
  port_setreg(p,0x0A,0x60);            // enable slow decay mode
  port_setreg(p,0x0D,0x60);            //
  port_setreg(p,0x3C,0x16);            // connection PWM0 PWR DOWN Input to 3-bit LUT9 OUT
  port_setreg(p,0xB6,0x09);            // set PWM0 to flex-Div clock
  port_setreg(p,0x4C,0x70);            // clear fault flags
  port_setreg(p,0x4C,0x7F);            // enable fault flags
  }

// use a single ISR for all the port UARTs
static void port_uart_irq(int pn);

// each port UART ISR calls the common ISR with an argument
static void port0_uart_irq() { port_uart_irq(0); }
static void port1_uart_irq() { port_uart_irq(1); }
static void port2_uart_irq() { port_uart_irq(2); }
static void port3_uart_irq() { port_uart_irq(3); }

// for PIO state machines
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
    mqhead[i]=mqtail[i]=0;                         // empty message queue
    p=porthw+i;
    gpio_init(p->pin_rts); gpio_set_dir(p->pin_rts,1); gpio_put(p->pin_rts,0);
    gpio_init(p->pin_dio); gpio_set_dir(p->pin_dio,0);
    gpio_init(p->pin_rx ); gpio_set_dir(p->pin_rx ,0);
    gpio_init(p->pin_tx ); gpio_set_dir(p->pin_tx ,0);
    portinfo[i].selmode=-1;
    portinfo[i].selreprate=-2;
    }
  gpio_put(PIN_PORTON,1);                          // enable port power
#ifdef DEBUG_PINS
  gpio_init(PIN_DEBUG0); gpio_set_dir(PIN_DEBUG0,1);
  gpio_init(PIN_DEBUG1); gpio_set_dir(PIN_DEBUG1,1);
#endif

  wait_ticks(100);
  ostr("Resetting drivers: ");
  for(i=0;i<NPORTS;i++) {
    odec(i);
    port_resetdriver(i);
    }
  onl();
  wait_ticks(100);

  ostr("Reading driver dumps: before port_initdriver(): ");
  for(i=0;i<NPORTS;i++) {
    odec(i);
    for(j=0;j<DRIVERBYTES;j++) {
      driverdata[i][j]=port_readi2cbyte(i,j);
      }
    }
  onl();

  for(i=0;i<NPORTS;i++) port_initdriver(i);

  ostr("Reading driver dumps: after port_initdriver(): ");
  for(i=0;i<NPORTS;i++) {
    odec(i);
    for(j=0;j<DRIVERBYTES;j++) {
      driverdata[i][j]=port_readi2cbyte(i,j);
      }
    }
  onl();

// configure PIOs for port UARTs
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
  irq_set_exclusive_handler(PIO0_IRQ_0,port0_uart_irq);
  irq_set_exclusive_handler(PIO0_IRQ_1,port1_uart_irq);
  irq_set_exclusive_handler(PIO1_IRQ_0,port2_uart_irq);
  irq_set_exclusive_handler(PIO1_IRQ_1,port3_uart_irq);

  for(i=0;i<NPORTS;i++) port_initpwm(i);
  ostrnl("Done initialising ports");
  }

// ========================== LPF2 port UARTs and ISRs =========================

// set the baud rate on a port UART
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
  float div=(float)clock_get_hz(clk_sys)/(8*2400);         // SMs transmit 1 bit per 8 execution cycles; dummy baud rate

// TX init
  pio_sm_set_pins_with_mask(pio,tsm,1<<txp,1<<txp);        // tell PIO to initially drive output-high on the selected pin, then map PIO
  pio_sm_set_pindirs_with_mask(pio,tsm,1<<txp,1<<txp);     // onto that pin with the IO muxes
  pio_gpio_init(pio,txp);
  c=uart_tx_program_get_default_config(txprogoffset0);
  sm_config_set_out_shift(&c,true,false,32);               // OUT shifts to right, no autopull

  sm_config_set_out_pins(&c, txp, 1);                      // we map both OUT and side-set to the same pin, because sometimes we need to assert
  sm_config_set_sideset_pins(&c, txp);                     // user data onto the pin (with OUT) and sometimes constant values (start/stop bit)
  sm_config_set_clkdiv(&c,div);

  pio_sm_init(pio,tsm,txprogoffset0,&c);
  pio_sm_set_enabled(pio,tsm,true);

// RX init
  pio_sm_set_consecutive_pindirs(pio,rsm,rxp,1,false);
  pio_gpio_init(pio,rxp);
  gpio_pull_up(rxp);

  c=uart_rx_program_get_default_config(rxprogoffset0);
  sm_config_set_in_pins(&c,rxp);                           // for WAIT, IN
  sm_config_set_jmp_pin(&c,rxp);                           // for JMP
  sm_config_set_in_shift(&c, true, false, 32);             // shift to right, autopull disabled
  sm_config_set_clkdiv(&c,div);

  q->mstate=MS_NOSYNC;                                     // initialise message processing variables
  q->lasttick=gettick();
  q->framingerrors=0;
  q->checksumerrors=0;
  q->txptr=-1;
  q->txlen=0;

  pio_sm_init(pio,rsm,rxprogoffset0,&c);
  switch(pn) {                                             // configure interrupts
case 0: pio0->inte0=0x12; break;
case 1: pio0->inte1=0x48; break;
case 2: pio1->inte0=0x12; break;
case 3: pio1->inte1=0x48; break;
    }
  pio_sm_set_enabled(pio,rsm,true);
  irq_set_enabled(p->irq,1);
  }

// switch off UART for a port
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
  switch(pn) {
case 0: pio0->inte0=0; break;
case 1: pio0->inte1=0; break;
case 2: pio1->inte0=0; break;
case 3: pio1->inte1=0; break;
    }
  q->mstate=MS_NOSYNC;
  q->txptr=-2;
  }

// wait for a character to be received on a port
int port_waitch(int pn) {
  struct porthw*p=porthw+pn;
  PIO pio=p->pio;
  int rsm=p->rxsm;
  while(pio_sm_is_rx_fifo_empty(pio,rsm)) ;
  return (int)*((io_rw_8*)&pio->rxf[rsm]+3);
  }

// port UART common ISR
static void port_uart_irq(int pn) {
  struct porthw*p=porthw+pn;
  struct portinfo*q=portinfo+pn;
  PIO pio=p->pio;
  int tsm=p->txsm;
  int rsm=p->rxsm;
  struct message*m=messages+pn;
  int b,i,f0,f1,f2;

  if(!pio_sm_is_tx_fifo_full(pio,tsm)) {                   // can we send something?
    if(q->txptr>=0) {
      pio_sm_put_blocking(pio,tsm,q->txbuf[q->txptr++]);   // send next character if any
      if(q->txptr==q->txlen) {                             // finished?
        q->txptr=-1;
        *p->inte=p->intb&PORT_INTE_RXMASK;                 // keep only receive interrupt enabled now
        }
      }
    else
      *p->inte=p->intb&PORT_INTE_RXMASK;                   // keep only receive interrupt enabled now
    }

  if(!pio_sm_is_rx_fifo_empty(pio,rsm)) {                  // RX not empty?
    b=(int)*((io_rw_8*)&pio->rxf[rsm]+3);                  // get byte
    m->check^=b;                                           // accumulate checksum
  DEB_SER  o2hex(b);
    switch(q->mstate) {
  case MS_NOSYNC:
  DEB_SER    o1ch('n');
      if(gettick()-q->lasttick<100) break;                 // wait for a pause
      q->mstate=MS_MTYPE;
      // and fall through
  case MS_MTYPE:
  DEB_SER    o1ch('t');
      m->check=b;                                          // initialise checksum
      f0=(b>>6)&3;                                         // split byte into three fields
      f1=(b>>3)&7;
      f2=(b>>0)&7;
      m->type=b&0xc7;                                      // extract type
      m->cmd=0;
      m->mode=0;
      m->plen=0;
      switch(f0) {                                         // examine top two bits
    case 0:                                                // "system message"
        switch(f2) {
      case 0:
      case 1:
      case 2:                                              // NACK
      case 3:
      case 6:
      case 7:
          break;                                           // SYNC-type messages ignored
      case 4:                                              // ACK
  DEB_SER        ostr("ACK");
          m->check=0xff;
          goto complete;
      case 5:                                              // PRG
          m->plen=1<<f1;
          q->mstate=MS_CMD;
          break;
          }
        break;
    case 1:                                                // "command message"
        m->plen=1<<f1;
        q->mstate=MS_PAYLOAD;                              // no extra command byte in this case
        break;
    case 2:                                                // "info message"
        m->mode=m->type&7;                                 // move mode bits to "mode"
        m->type&=~7;
        m->plen=1<<f1;
        q->mstate=MS_CMD;                                  // command byte comes next
        break;
    case 3:                                                // "data message"
        m->mode=m->type&7;                                 // move mode bits to "mode"
        m->type&=~7;
        m->plen=1<<f1;
        q->mstate=MS_PAYLOAD;                              // payload comes next
        break;
        }
      break;
  case MS_CMD:
  DEB_SER    o1ch('c');
      m->cmd=b;
      m->mode|=(b>>2)&0x08;                                // extended mode bit
      q->mstate=MS_PAYLOAD;
      break;
  default:
  DEB_SER    o1ch('p');
      if(q->mstate<(int)m->plen) {                         // mstate goes from 0 to plen-1 as payload bytes are read
        m->payload[q->mstate++]=b;
        break;
        }
  complete:
// here we have a complete message with its checksum incorporated into m->check
      if(m->check!=0xff) q->checksumerrors++;
      else {                                               // if checksum OK add to message queue
        i=(mqhead[pn]+1)%MQLEN;
        if(i!=mqtail[pn]) {                                // discard on overrunning the message queue
          memcpy((void*)&mqueue[pn][mqhead[pn]],m,sizeof(struct message));
          mqhead[pn]=i;
        } else {
          ostrnl("MQ overrun!");
          }
        }
      q->mstate=MS_MTYPE;                                  // ready for next message
      break;
      }
    q->lasttick=gettick();
    }
  }

// send a message to a given port
void port_sendmessage(int pn,unsigned char*buf,int l) {
  struct porthw*p=porthw+pn;
  struct portinfo*q=portinfo+pn;
  if(l<1) return;                                          // no message
  if(l>TXBLEN) l=TXBLEN;
  if(q->txptr==-2) return;                                 // UART off? ignore
  while(q->txptr>=0) ;                                     // wait for TX idle
  memcpy((void*)q->txbuf,buf,l);                           // copy message to transmit buffer
  q->txptr=0;
  q->txlen=l;
  *p->inte=p->intb;                                        // enable both interrupts
  switch(pn) {
case 0:pio0->inte0=0x12; break;
case 1:pio0->inte1=0x48; break;
case 2:pio1->inte0=0x12; break;
case 3:pio1->inte1=0x48; break;
    }
  }

