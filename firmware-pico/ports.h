#pragma once

#define DRIVERBYTES 256                            // driver configuration bits address space

#define TXBLEN 132                                 // maximum message length
#define PWM_PERIOD_DEFAULT 32768                   // in units of 10ns
extern unsigned int pwm_period;

extern struct portinfo {
  unsigned int lasttick;                           // tick when last character seen
  volatile int mstate;                             // message reading state
  volatile int framingerrors;                      // error counters
  volatile int checksumerrors;
  volatile unsigned char txbuf[TXBLEN];            // buffer to send
  volatile int txptr;                              // current index into buffer; -1 for idle
  volatile int txlen;                              // buffer length
  int pwmmode;                                     // 0=direct PWM, 1=PID controlled
  float setpoint;                                  // PID/direct PWM setpoint
  int spwaveshape;                                 // set point wave generator
  float spwavemin;
  float spwavemax;
  float spwaveperiod;
  float spwavephase;
  float spwavephaseacc;
  float pid_pv;                                    // PID process variable
  float pid_pv_last;                               // previous value of process variable as read from sensor
  float pid_ierr;                                  // PID integral error accumulator
  float pid_perr;                                  // PID previous error
  unsigned int pvport;
  unsigned int pvmode;
  unsigned int pvoffset;
  unsigned int pvformat;
  float pvscale;
  float pvunwrap;
  float Kp,Ki,Kd;                                  // PID coefficients Q16
  float windup;                                    // windup limit for integral error
  } portinfo[NPORTS];

extern struct message {
  UC type;                                         // first byte with length and mode information zeroed
  UC cmd;                                          // second byte if any
  UC mode;                                         // mode if any
  UC plen;                                         // payload length (0..128)
  UC payload[128];
  UC check;                                        // checksum (0xff for pass)
  } messages[NPORTS];

#define MQLEN 4
extern volatile struct message mqueue[NPORTS][MQLEN];
extern volatile int mqhead[NPORTS],mqtail[NPORTS]; // if head==tail then the queue is empty

extern void port_initpwm(unsigned int period);
extern void init_ports();
extern unsigned int port_state56(int pn);
extern void port_set_pwm(int pn,float pwm);
extern void port_motor_brake(int pn);

// ========================== LPF2 port UARTs and ISRs =========================

extern void port_setbaud(int pn,int baud);
extern void port_uarton(int pn);
extern void port_uartoff(int pn);
extern void port_putch(int pn,int c);
extern int port_getch(int pn);
extern int port_waitch(int pn);
extern void port_uart_irq(int pn);
extern void port_sendmessage(int pn,unsigned char*buf,int l);

