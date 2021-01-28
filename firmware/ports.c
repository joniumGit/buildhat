#include <string.h>
#include "common.h"
#include "system.h"
#include "debug.h"
#include "control.h"
#include "ioconv.h"
#include "timer.h"
#include "hardware.h"
#include "gpio.h"
#include "ports.h"

unsigned int pwm_period=PWM_PERIOD_DEFAULT;

struct portinfo portinfo[NPORTS];
struct message messages[NPORTS];
volatile struct message mqueue[NPORTS][MQLEN];
volatile int mqhead[NPORTS],mqtail[NPORTS];        // if head==tail then the queue is empty

void port_initpwm(unsigned int period) {
  struct porthw*p;
  int i,j;
  pwm_period=period;
  for(i=0;i<NPORTS;i++) {
    for(j=0;j<i;j++)
      if(porthw[i].timer==porthw[j].timer)         // have we already initialised this one?
        goto skip;                                 // then don't do it again
    porthw[i].timer->CR1=0x00;                     // non-buffered ARR, edge-aligned PWM, disabled
    porthw[i].timer->CCMR1=0x6868;                 // ch1, ch2 configured as PWM outputs, preload enabled
    porthw[i].timer->CCMR2=0x6868;                 // ch3, ch4 configured as PWM outputs, preload enabled
    porthw[i].timer->CCER=0x3333;                  // enable all outputs, all inverted
    porthw[i].timer->ARR=period-1;
    porthw[i].timer->CCR1=0;                       // set all outputs to 0%
    porthw[i].timer->CCR2=0;
    porthw[i].timer->CCR3=0;
    porthw[i].timer->CCR4=0;
    porthw[i].timer->BDTR=0x8000;                  // main output enable (only on "advanced" timer; writing
                                                   // does not seem to cause harm on general-purpose timer)
    porthw[i].timer->CR1=0x01;                     // non-buffered ARR, edge-aligned PWM, enabled
    porthw[i].timer->EGR=1;                        // set UG bit to load registers
skip: ;
    }
  for(i=0;i<NPORTS;i++) {
    p=porthw+i;
    gpio_afr(p->port_in1,p->pin_in1,p->tim_afn);   // set up all PWM outputs
    gpio_afr(p->port_in2,p->pin_in2,p->tim_afn);
    }
  }

void init_ports() {
  int i;
  struct porthw*p;
  RCC->APB1ENR|=APB1CLOCKS_P;
  RCC->APB2ENR|=APB2CLOCKS_P;
  gpio_out0(GPIOC,5);
  for(i=0;i<NPORTS;i++) {
    mqhead[i]=mqtail[i]=0;
    p=porthw+i;
    gpio_out0(p->port_en ,p->pin_en );
    gpio_out0(p->port_rts,p->pin_rts);
    gpio_in  (p->port_d5 ,p->pin_d5 );
    gpio_in  (p->port_d6 ,p->pin_d6 );
    gpio_in  (p->port_rx ,p->pin_rx );
    gpio_out0(p->port_tx ,p->pin_tx );
    gpio_out0(p->port_in1,p->pin_in1);
    gpio_out0(p->port_in2,p->pin_in2);
    }
  gpio_out1(GPIOC,5);                              // enable port power
  wait_ticks(100);
  for(i=0;i<NPORTS;i++) {
    p=porthw+i;
    gpio_out1(p->port_en ,p->pin_en );             // disable all drivers
    }
#ifdef DEBUG_PINS
  gpio_out0(GPIOD,3);
  gpio_out0(GPIOD,4);
#endif

  port_initpwm(PWM_PERIOD_DEFAULT);

  nvic_intena(USART1_IRQn);                        // enable all UART interrupts
  nvic_intena(USART2_IRQn);
  nvic_intena(USART3_IRQn);
  nvic_intena( UART4_IRQn);
  nvic_intena( UART5_IRQn);
  nvic_intena(USART6_IRQn);
  nvic_intena( UART7_IRQn);
  nvic_intena( UART8_IRQn);
  nvic_intena( UART9_IRQn);
  nvic_intena(UART10_IRQn);
  }

unsigned int port_state56(int pn) {
  unsigned int u;
  struct porthw*p=porthw+pn;
  u=GPIO_READ(p->port_d6,p->pin_d6)+(GPIO_READ(p->port_d5,p->pin_d5)<<1);
  return u;
  }

// set PWM values according to pwm:
// -1 full power reverse
// +1 full power forwards
void port_set_pwm(int pn,float pwm) {
  struct porthw*p=porthw+pn;
  int u;
  CLAMP(pwm,-1,1);
  u=(int)(pwm*pwm_period+0.5);
  p->timer->CR1|=0x02;                             // disable updates while we write to the registers to avoid glitches
  if(u<0) *(p->tccr1)=0, *(p->tccr2)=-u;
  else    *(p->tccr2)=0, *(p->tccr1)= u;
  p->timer->CR1&=~0x02;                            // re-enable updates
  }

void port_motor_brake(int pn) {
  struct porthw*p=porthw+pn;
  *(p->tccr1)=pwm_period;
  *(p->tccr2)=pwm_period;
  }

// ========================== LPF2 port UARTs and ISRs =========================

void port_setbaud(int pn,int baud) {
  struct porthw*p=porthw+pn;
  USART_TypeDef*u=p->uart;
  if(baud<  2400) baud=  2400;
  if(baud>115200) baud=115200;
  u->BRR=(100000000/baud+1)/2;
  }

// message state machine
#define MS_NOSYNC  -3
#define MS_MTYPE   -2
#define MS_CMD     -1
#define MS_PAYLOAD  0

void port_uarton(int pn) {
  struct porthw*p=porthw+pn;
  struct portinfo*q=portinfo+pn;
  USART_TypeDef*u=p->uart;
  gpio_out0(p->port_en ,p->pin_en );               // enable driver
  gpio_afr(p->port_rx,p->pin_rx,p->uart_afn);      // enable UART functions on pins
  gpio_afr(p->port_tx,p->pin_tx,p->uart_afn);
  port_setbaud(pn,2400);
  q->mstate=MS_NOSYNC;
  q->lasttick=tick;
  q->framingerrors=0;
  q->checksumerrors=0;
  q->txptr=-1;
  q->txlen=0;
  u->CR1=0x202c;                                   // enable USART, TX and RX, TX and RX interrupts
  }

void port_uartoff(int pn) {
  struct porthw*p=porthw+pn;
  struct portinfo*q=portinfo+pn;
  USART_TypeDef*u=p->uart;
  u->CR1=0;                                        // disable USART, TX and RX, interrupts
  gpio_in  (p->port_rx ,p->pin_rx );               // return pins to normal functions
  gpio_in  (p->port_tx ,p->pin_tx );
  gpio_out1(p->port_en ,p->pin_en );               // disable driver
  q->mstate=MS_NOSYNC;
  q->txptr=-1;
  }

void port_putch(int pn,int c) {
  struct porthw*p=porthw+pn;
  USART_TypeDef*u=p->uart;
  while((u->SR&0x80)==0) ;
  u->DR=c;
  }

int port_getch(int pn) {
  struct porthw*p=porthw+pn;
  USART_TypeDef*u=p->uart;
  if(u->SR&0x20) return u->DR;
  return -1;
  }

int port_waitch(int pn) {
  struct porthw*p=porthw+pn;
  USART_TypeDef*u=p->uart;
  while((u->SR&0x20)==0) ;
  return u->DR;
  }

void port_uart_irq(int pn) {
  struct porthw*p=porthw+pn;
  struct portinfo*q=portinfo+pn;
  USART_TypeDef*u=p->uart;
  struct message*m=messages+pn;
  int b,i,s,f0,f1,f2;
  s=u->SR;
  if(s&0x80) {                                     // TX empty?
    if(q->txptr>=0) {
      u->DR=q->txbuf[q->txptr++];                  // send next character
      if(q->txptr==q->txlen) {                     // finished?
        q->txptr=-1;
        u->CR1&=~0x80;
        }
      }
    else
      u->CR1&=~0x80;
    }
  if(s&0x02) {                                     // framing error? this detects break condition
    q->framingerrors++;
DEB_SER    ostrnl("*FrE*");
    b=u->DR;                                       // clear framing error
    q->mstate=MS_MTYPE;
    return;
    }
  if(s&0x20) {                                     // RX not empty?
    b=u->DR;
    m->check^=b;                                   // accumulate checksum
//    GPIOD->ODR|=8;
  DEB_SER  o2hex(b);
  //  o1ch('<'); o2hex(b); osp();
    switch(q->mstate) {
  case MS_NOSYNC:
  DEB_SER    o1ch('n');
      if(tick-q->lasttick<100) break;              // wait for a pause
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
    q->lasttick=tick;
    }
//  GPIOD->ODR&=~8;
  }

void port_sendmessage(int pn,unsigned char*buf,int l) {
  struct porthw*p=porthw+pn;
  struct portinfo*q=portinfo+pn;
  if(l<1) return;
  if(l>TXBLEN) l=TXBLEN;
  while(q->txptr>=0) ;                             // wait for TX idle
  memcpy((void*)q->txbuf,buf,l);
  q->txptr=0;
  q->txlen=l;
  p->uart->CR1|=0x80;
  }

