// Accept, parse and process commands received over the control port UART

#include "common.h"
#include "hardware.h"
#include "ports.h"
#include "device.h"
#include "message.h"
#include "pwm_pid.h"
#include "adc.h"
#include "debug.h"
#include "ioconv.h"
#include "control.h"

static int cmdport=0;                                     // current port affected by commands

static void cmd_help() {
  ostrnl("Commands available:");
  ostrnl("  help, ?            : display this text");
  ostrnl("  port <port>        : select a port (default 0)");
  ostrnl("  vin                : report main power input voltage");
  ostrnl("  pwm                : set current port to direct PWM mode (default)");
  ostrnl("  off                : same as pwm ; set 0");
  ostrnl("  on                 : same as pwm ; set 1");
  ostrnl("  pwmfreq <freq>     : set PWM frequency (affects all ports)");
  ostrnl("  pid <pidparams>    : set current port to PID control mode with <pidparams>");
  ostrnl("  set <setpoint>     : configure constant set point for current port");
  ostrnl("  set <waveparams>   : configure varying set point for current port");
  ostrnl("  debug <debugcode>  : enable debugging output");
  ostrnl("  xyzzy              : remove PID output drive limit");
  ostrnl("");
  ostrnl("Where:");
  ostr  ("  <port>             : 0.."); odec(NPORTS-1); onl();
  ostrnl("  <setpoint>         : -1..+1 for direct PWM; unrestricted for PID control");
  ostrnl("  <freq>             : 0=6kHz (default); 1=12kHz; 2=24kHz; 3=48kHz");
  ostrnl("  <pidparams>        : <pvport> <pvmode> <pvoffset> <pvformat> <pvscale> <pvunwrap> <Kp> <Ki> <Kd> <windup>");
  ostrnl("    <pvport>         : port to fetch process variable from");
  ostrnl("    <pvmode>         : mode to fetch process variable from");
  ostrnl("    <pvoffset>       : process variable byte offset into mode");
  ostrnl("    <pvformat>       : u1=unsigned byte;  s1=signed byte;");
  ostrnl("                       u2=unsigned short; s2=signed short;");
  ostrnl("                       u4=unsigned int;   s4=signed int;");
  ostrnl("                       f4=float");
  ostrnl("    <pvscale>        : process variable multiplicative scale factor");
  ostrnl("    <pvunwrap>       : 0=no unwrapping; otherwise modulo for process variable phase unwrap");
  ostrnl("    <Kp>, <Ki>, <Kd> : PID controller gains (Δt=1s)");
  ostrnl("    <windup>         : PID integral windup limit");
  ostrnl("  <waveparams>       : <shape> <min> <max> <period> <phase>");
  ostrnl("    <shape>          : square | sine | triangle");
  ostrnl("  <debugcode>        : OR of 1=serial port; 2=connect/disconnect; 4=signature;");
  ostrnl("                       8=DATA payload; 16=PID controller");
  }

static int cmd_port() {
  unsigned int u;
  if(!parseuint(&u)) return 1;
  CLAMP(u,0,NPORTS-1);
  cmdport=u;
  return 0;
  }
static int cmd_pwm()  {
  portinfo[cmdport].setpoint=0;
  portinfo[cmdport].pwmmode=0;
  return 0;
  }
static int cmd_pwmfreq()  {
  unsigned int u;
  if(!parseuint(&u)) return 1;
  CLAMP(u,0,3);
  port_initpwm(PWM_PERIOD_DEFAULT>>u);
  return 0;
  }
static int cmd_pid()  {
  unsigned int u;
  float v;
  portinfo[cmdport].setpoint=0;
  if(!parseuint(&u))  goto err; CLAMP(u,0,NPORTS-1);                       portinfo[cmdport].pvport=u;
  if(!parseuint(&u))  goto err; CLAMP(u,0,MAXNMODES-1);                    portinfo[cmdport].pvmode=u;
  if(!parseuint(&u))  goto err; CLAMP(u,0,127);                            portinfo[cmdport].pvoffset=u;
  if(!parsepvfmt(&u)) goto err;                                            portinfo[cmdport].pvformat=u;
  if(!parsefloat(&v)) goto err; CLAMP(u,-32,32);                           portinfo[cmdport].pvscale=v;
  if(!parsefloat(&v)) goto err;                                            portinfo[cmdport].pvunwrap=v;
  if(!parsefloat(&v)) goto err;                                            portinfo[cmdport].Kp=v;
  if(!parsefloat(&v)) goto err;                                            portinfo[cmdport].Ki=v;
  if(!parsefloat(&v)) goto err;                                            portinfo[cmdport].Kd=v;
  if(!parsefloat(&v)) goto err;                                            portinfo[cmdport].windup=v;
  portinfo[cmdport].pwmmode=1;                     // enable PID controller and trigger messages from device providing process variable
//!!!  device_sendselect(portinfo[cmdport].pvport,portinfo[cmdport].pvmode);
  return 0;
err:
  portinfo[cmdport].pwmmode=0;
  return 1;
  }
static void cmd_set_const(float u) {
  portinfo[cmdport].spwaveshape =WAVE_SQUARE;
  portinfo[cmdport].spwavemin   =u;
  portinfo[cmdport].spwavemax   =u;
  portinfo[cmdport].spwaveperiod=1;
  portinfo[cmdport].spwavephase =0;
  }
static int cmd_set_wave(int shape) {
  float min,max,period,phase;
  if(!parsefloat(&min))    return 1;
  if(!parsefloat(&max))    return 1;
  if(!parsefloat(&period)) return 1;
  if(!parsefloat(&phase))  return 1;
  CLAMP(period,2*PWM_UPDATE/1000.0,1e30);
  CLAMP(phase,0,1);
  portinfo[cmdport].spwaveshape =shape;
  portinfo[cmdport].spwavemin   =min;
  portinfo[cmdport].spwavemax   =max;
  portinfo[cmdport].spwaveperiod=period;
  portinfo[cmdport].spwavephase =phase;
  return 0;
  }
static int cmd_set()  {
  float u;
  if(strmatch("square"))   return cmd_set_wave(WAVE_SQUARE);
  if(strmatch("sine"))     return cmd_set_wave(WAVE_SINE);
  if(strmatch("triangle")) return cmd_set_wave(WAVE_TRI);
  if(!parsefloat(&u)) return 1;
  cmd_set_const(u);
  return 0;
  }
static int cmd_on()    { cmd_set_const(1.0); return 0; }
static int cmd_off()   { cmd_set_const(0.0); return 0; }
//!!! static int cmd_vin()   { ofxp(read_vin(),16,2); ostrnl(" V"); return 0; }
static int cmd_vin()   { ofxp(1234567,16,2); ostrnl(" V"); return 0; }
static int cmd_debug() { return !parseint(&debug); }
static int cmd_xyzzy() { pid_drive_limit=1.0; return 0; }

void cmd_prompt() { o1ch('P'); odec(cmdport); o1ch('>'); }

void proc_cmd() {
  parse_reset();
  for(;;) {
    skspsc();
    if(parse_eol()) goto done;
    else if(strmatch("help"   )) cmd_help();
    else if(strmatch("?"      )) cmd_help();
    else if(strmatch("port"   )) { if(cmd_port())    goto err; }
    else if(strmatch("pwm"    )) { if(cmd_pwm())     goto err; }
    else if(strmatch("pwmfreq")) { if(cmd_pwmfreq()) goto err; }
    else if(strmatch("pid"    )) { if(cmd_pid())     goto err; }
    else if(strmatch("set"    )) { if(cmd_set())     goto err; }
    else if(strmatch("off"    )) { if(cmd_off())     goto err; }
    else if(strmatch("on"     )) { if(cmd_on())      goto err; }
    else if(strmatch("vin"    )) { if(cmd_vin())     goto err; }
    else if(strmatch("debug"  )) { if(cmd_debug())   goto err; }
    else if(strmatch("xyzzy"  )) { if(cmd_xyzzy())   goto err; }
    else goto err;
    }
err:
  ostrnl("Error");
done:
  cmd_prompt();
  }
