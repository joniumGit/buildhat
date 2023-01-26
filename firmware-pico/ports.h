#pragma once

#define DRIVERBYTES 256                            // driver configuration bits address space

#define TXBLEN 132                                 // maximum message length
#define PWM_PERIOD 255                             // in units of ~40μs/255

#define DEFAULT_SELREPRATE 100

#define PWM_UPDATE 10                              // in milliseconds

extern int pwm_drive_limit;

struct svar {
  unsigned int port;
  unsigned int mode;
  unsigned int offset;
  unsigned int format;
  float scale;
  float unwrap;
  float last;
  };

extern struct portinfo {
  unsigned int lasttick;                           // tick when last character seen
  volatile int mstate;                             // message reading state
  volatile int framingerrors;                      // error counters
  volatile int checksumerrors;
  volatile unsigned char txbuf[TXBLEN];            // buffer to send
  volatile int txptr;                              // current index into buffer; -1 for idle; -2 for UART off
  volatile int txlen;                              // buffer length

  int selmode;                                     // mode selected by "select" command or -1 for none
  int seloffset;                                   // offset of variable to report, or -1 for all mode data
  int selformat;                                   // format of mode data (int8/int32/float etc.)
  unsigned int selrxcount;                         // incremented on each message received
  int selrxever;                                   // have we ever received a message for this mode since selecting it?
  int selreprate;                                  // report rate for select messages: -2: disabled; -1: once only; 0: as received; ≥1: target interval between reports in ms

  int pwmmode;                                     // 0=direct PWM, 1=PID controlled, 2=PID controlled with differentiator on process variable
  int coast;                                       // 1=coasting
  int lastpwm;                                     // integer value to which PWM was last set
  int pwmthresh;                                   // slow/fast PWM switchover thresholed Q16
  int pwmthreshacc;                                // slow PWM error accumulator Q16
  int minpwm;                                      // minimum PWM driver input value
  float setpoint;                                  // PID/direct PWM setpoint
  int spwaveshape;                                 // set point wave generator
  float spwavemin;
  float spwavemax;
  float spwaveperiod;
  float spwavephase;
  float spwavephaseacc;
  float pid_pv;                                    // PID process variable
  float pid_ierr;                                  // PID integral error accumulator
  float pid_perr;                                  // PID previous error
  struct svar pvsvar;
  float Kp,Ki,Kd;                                  // PID coefficients Q16
  float windup;                                    // windup limit for integral error
  float deadzone;                                  // ± dead zone for error
  } portinfo[NPORTS];

extern struct message {
  UC type;                                         // first byte with length and mode information zeroed
  UC cmd;                                          // second byte if any
  UC mode;                                         // mode if any
  UC plen;                                         // payload length (0..128)
  UC payload[128];
  UC check;                                        // checksum (0xff for pass)
  UC pad[3];
  } messages[NPORTS];

#define MQLEN 4
extern volatile struct message mqueue[NPORTS][MQLEN];
extern volatile int mqhead[NPORTS],mqtail[NPORTS]; // if head==tail then the queue is empty

extern void port_initpwm();
extern void init_ports();
extern void port_clearfaults();
extern void port_checkmfaults();
extern void port_driverdump();
extern unsigned int port_state56(int pn);
extern void port_set_pwm(int pn,float pwm);
extern void port_motor_coast(int pn);

extern void init_accel();
// extern void accel_getaxyz(int*ax,int*ay,int*az);

// ========================== LPF2 port UARTs and ISRs =========================

extern void port_setbaud(int pn,int baud);
extern void port_uarton(int pn);
extern void port_uartoff(int pn);
//extern void port_putch(int pn,int c);
//extern int port_getch(int pn);
extern int port_waitch(int pn);
extern void port_sendmessage(int pn,unsigned char*buf,int l);

