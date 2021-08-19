// Accept, parse and process commands received over the control port UART

#include "common.h"
#include "hardware.h"
#include "ports.h"
#include "device.h"
#include "message.h"
#include "pwm_pid.h"
#include "timer.h"
#include "debug.h"
#include "ioconv.h"
#include "control.h"
#include "version.h"
//#include "hardware/resets.h"

extern void reboot();

static int cmdport=0;                                     // current port affected by commands

static void cmd_help() {
  ostrnl("Commands available:");
  ostrnl("  help, ?              : display this text");
  ostrnl("  port <port>          : select a port (default 0)");
  ostrnl("  list                 : list connected devices");
  ostrnl("  vin                  : report main power input voltage");
  ostrnl("  ledmode <ledmode>    : set LED function");
  ostrnl("  clear_faults         : clear latched motor fault conditions");
  ostrnl("  coast                : disable motor driver");
  ostrnl("  pwm                  : set current port to direct PWM mode (default)");
  ostrnl("  off                  : same as pwm ; set 0");
  ostrnl("  on                   : same as pwm ; set 1");
  ostrnl("  pid <pidparams>      : set current port to PID control mode with <pidparams>");
  ostrnl("  set <setpoint>       : configure constant set point for current port");
  ostrnl("  set <waveparams>     : configure varying set point for current port");
  ostrnl("  bias <bias>          : set bias offset for motor drive on current port (default 0)");
  ostrnl("  plimit <limit>       : set PWM output drive limit for all ports (default 0.1) after bias mapping");
  ostrnl("  select <selvar>      : send a SELECT message to select a variable and output it");
  ostrnl("  select <selmode>     : send a SELECT message to select a mode and output all its data in raw hex");
  ostrnl("  select               : stop outputting data");
  ostrnl("  selonce <selvar>     : as 'select' but only report one data packet");
  ostrnl("  selonce <selmode>    : as 'select' but only report one data packet");
  ostrnl("  combi <index> <clist>: configure a combi mode with a list of mode/dataset specifiers");
  ostrnl("  combi <index>        : de-configure a combi mode");
//  ostrnl("  accelerometer        : read accelerometer data once");
  ostrnl("  write1 <hexbyte>*    : send message with 1-byte header; pads if necessary, sets payload length and checksum");
  ostrnl("  write2 <hexbyte>*    : send message with 2-byte header; pads if necessary, sets payload length and checksum");
  ostrnl("  echo <0|1>           : enable/disable echo and prompt on command port");
  ostrnl("  debug <debugcode>    : enable debugging output");
  ostrnl("  version              : print version string");
  ostrnl("  signature            : dump firmware signature");
//  ostrnl("  bootloader           : reset into bootloader");
  ostrnl("");
  ostrnl("Where:");
  ostr  ("  <port>               : 0.."); odec(NPORTS-1); onl();
  ostrnl("  <ledmode>            : 0=off 1=orange 2=green 3=orange+green –1=monitor Vin (default)");
  ostrnl("  <setpoint>           : –1..+1 for direct PWM; unrestricted for PID control");
  ostrnl("  <pidparams>          : <pvport> <pvmode> <pvoffset> <pvformat> <pvscale> <pvunwrap> <Kp> <Ki> <Kd> <windup>");
  ostrnl("    <pvport>           : port to fetch process variable from");
  ostrnl("    <pvmode>           : mode to fetch process variable from");
  ostrnl("    <pvoffset>         : process variable byte offset into mode");
  ostrnl("    <pvformat>         : u1=unsigned byte;  s1=signed byte;");
  ostrnl("                         u2=unsigned short; s2=signed short;");
  ostrnl("                         u4=unsigned int;   s4=signed int;");
  ostrnl("                         f4=float");
  ostrnl("    <pvscale>          : process variable multiplicative scale factor");
  ostrnl("    <pvunwrap>         : 0=no unwrapping; otherwise modulo for process variable phase unwrap");
  ostrnl("    <Kp>, <Ki>, <Kd>   : PID controller gains (Δt=1s)");
  ostrnl("    <windup>           : PID integral windup limit");
  ostrnl("  <waveparams>         : square   <min> <max> <period> <phase>");
  ostrnl("                       | sine     <min> <max> <period> <phase>");
  ostrnl("                       | triangle <min> <max> <period> <phase>");
  ostrnl("                       | pulse    <during> <after> <length> 0");
  ostrnl("                       | ramp     <from> <to> <duration> 0");
  ostrnl("  <limit>              : 0..1 as fraction of maximum PWM drive");
  ostrnl("  <bias>               : 0..1 set biased motor drive mapping");
  ostrnl("                         (0,+1] offset by +bias");
  ostrnl("                         0 mapped to 0");
  ostrnl("                         [–1,0) offset by –bias");
  ostrnl("  <selvar>             : <selmode> <seloffset> <selformat>");
  ostrnl("    <selmode>          : mode to fetch variable from");
  ostrnl("    <seloffset>        : variable byte offset into mode");
  ostrnl("    <selformat>        : u1=unsigned byte;  s1=signed byte;");
  ostrnl("                         u2=unsigned short; s2=signed short;");
  ostrnl("                         u4=unsigned int;   s4=signed int;");
  ostrnl("                         f4=float");
  ostrnl("  <clist>              : {<mode> <dataset>}*");
  ostrnl("  <hexbyte>            : 1- or 2-digit hex value");
  ostrnl("  <debugcode>          : OR of 1=serial port; 2=connect/disconnect; 4=signature;");
  ostrnl("                         8=DATA payload; 16=PID controller");
  }

static int cmd_port() {
  unsigned int u;
  if(!parseuint(&u)) return 1;
  CLAMP(u,0,NPORTS-1);
  cmdport=u;
  return 0;
  }
static int cmd_ledmode() {
  int u;
  if(!parseint(&u)) return 1;
  CLAMP(u,-1,3);
  ledmode=u;
  return 0;
  }
static int cmd_list()  {
  int i;
  for(i=0;i<NPORTS;i++) device_dump(i);
  return 0;
  }
static int cmd_coast()  {
  portinfo[cmdport].coast=1;
  return 0;
  }
static int cmd_pwm()  {
  portinfo[cmdport].setpoint=0;
  portinfo[cmdport].pwmmode=0;
  portinfo[cmdport].coast=0;
  return 0;
  }
static int cmd_pid()  {
  unsigned int u;
  float v;
  portinfo[cmdport].setpoint=0;
  if(!parseuint(&u))  goto err; CLAMP(u,0,NPORTS-1);                       portinfo[cmdport].pvport=u;
  if(!parseuint(&u))  goto err; CLAMP(u,0,MAXNMODES-1);                    portinfo[cmdport].pvmode=u;
  if(!parseuint(&u))  goto err; CLAMP(u,0,127);                            portinfo[cmdport].pvoffset=u;
  if(!parsefmt(&u))   goto err;                                            portinfo[cmdport].pvformat=u;
  if(!parsefloat(&v)) goto err; CLAMP(u,-32,32);                           portinfo[cmdport].pvscale=v;
  if(!parsefloat(&v)) goto err;                                            portinfo[cmdport].pvunwrap=v;
  if(!parsefloat(&v)) goto err;                                            portinfo[cmdport].Kp=v;
  if(!parsefloat(&v)) goto err;                                            portinfo[cmdport].Ki=v;
  if(!parsefloat(&v)) goto err;                                            portinfo[cmdport].Kd=v;
  if(!parsefloat(&v)) goto err;                                            portinfo[cmdport].windup=v;
  portinfo[cmdport].pwmmode=1;                     // enable PID controller and trigger messages from device providing process variable
  portinfo[cmdport].coast=0;
//  device_sendselect(portinfo[cmdport].pvport,portinfo[cmdport].pvmode);
  return 0;
err:
  portinfo[cmdport].pwmmode=0;
  portinfo[cmdport].coast=0;
  return 1;
  }
static void cmd_set_const(float u) {
  portinfo[cmdport].spwaveshape =WAVE_SQUARE;
  portinfo[cmdport].spwavemin   =u;
  portinfo[cmdport].spwavemax   =u;
  portinfo[cmdport].spwaveperiod=1;
  portinfo[cmdport].spwavephase =0;
  portinfo[cmdport].coast=0;
  }
static int cmd_set_wave(int shape) {
  float min,max,period,phase;
  if(!parsefloat(&min))    return 1;
  if(!parsefloat(&max))    return 1;
  if(!parsefloat(&period)) return 1;
  if(!parsefloat(&phase))  return 1;
  CLAMP(period,2*PWM_UPDATE/1000.0,1e30);
  CLAMP(phase,0,1);
  portinfo[cmdport].spwaveshape   =shape;
  portinfo[cmdport].spwavemin     =min;
  portinfo[cmdport].spwavemax     =max;
  portinfo[cmdport].spwaveperiod  =period;
  portinfo[cmdport].spwavephase   =phase;
  portinfo[cmdport].spwavephaseacc=0;
  portinfo[cmdport].coast=0;
  return 0;
  }
static int cmd_set()  {
  float u;
  if(strmatch("square"))   return cmd_set_wave(WAVE_SQUARE);
  if(strmatch("sine"))     return cmd_set_wave(WAVE_SINE);
  if(strmatch("triangle")) return cmd_set_wave(WAVE_TRI);
  if(strmatch("pulse"))    return cmd_set_wave(WAVE_PULSE);
  if(strmatch("ramp"))     return cmd_set_wave(WAVE_RAMP);
  if(!parsefloat(&u)) return 1;
  cmd_set_const(u);
  return 0;
  }
//static int cmd_bootloader() {
//  reset_block(0x01ffffff);
//  reboot();
//  }
static int cmd_on()          { cmd_set_const(1.0); return 0; }
static int cmd_off()         { cmd_set_const(0.0); return 0; }
static int cmd_vin()         { ofxp((adc_vin<<16)/1000,16,2); ostrnl(" V"); return 0; }
static int cmd_echo()        { return !parseint(&echo); }
static int cmd_debug()       { return !parseint(&debug); }
static int cmd_plimit()      {
  float u;
  if(!parsefloat(&u)) return 1;
  CLAMP(u,0,1);
  pwm_drive_limit=(int)(u*65536+0.5); // Q16
  return 0;
  }
static int cmd_bias()        {
  float u;
  if(!parsefloat(&u)) return 1;
  CLAMP(u,0,1);
  portinfo[cmdport].bias=(int)(u*65536+0.5); // Q16
  return 0;
  }
static int cmd_select(int f) { // f=0: normal mode; f=1 report value only once ("selonce" command)
  int u;
  if(!parseint(&u))  goto off; CLAMP(u,0,MAXNMODES-1); portinfo[cmdport].selmode=u;
  if(!parseint(&u))  goto raw; CLAMP(u,0,127);         portinfo[cmdport].seloffset=u;
  if(!parsefmt(&u))  goto err;                         portinfo[cmdport].selformat=u;
  portinfo[cmdport].selmodeonce=f;
  device_sendselect(cmdport,portinfo[cmdport].selmode);
  portinfo[cmdport].selrxcount=0;
  return 0;
off:
  portinfo[cmdport].selmode=-1;
  portinfo[cmdport].selrxcount=0;
  return 0;
raw:
  portinfo[cmdport].seloffset=-1;
  portinfo[cmdport].selmodeonce=f;
  device_sendselect(cmdport,portinfo[cmdport].selmode);
  portinfo[cmdport].selrxcount=0;
  return 0;
err:
  portinfo[cmdport].selmode=-1;
  portinfo[cmdport].selrxcount=0;
  return 1;
  }
static int cmd_combi() {
  int i,j;
  int n;                         // count of entries
  unsigned char b[128];
  struct modeinfo*mp;
  if(!parseint(&i)) goto err;
  CLAMP(i,0,7);
  b[0]=0x20;
  b[1]=i;
  mp=&devinfo[cmdport].modes[i];
  mp->combi_count=0;
  for(n=0;n<15;n++) {
    if(!parseint(&i)) break;
    CLAMP(i,0,15);
    if(!parseint(&j)) goto err;
    CLAMP(j,0,15);
    b[2+n]=(i<<4)+j;
    mp->combi_count=n+1;
    mp->combi_mode[n]=i;
    mp->combi_dataset[n]=j;
    }
  b[0]+=n;
  device_sendmessage(cmdport,0x44,-1,2+n,b);
  return 0;
err:
  return 1;
  }
static int cmd_write(int nh) {   // nh=number of header bytes, 1 or 2
  int i;
  unsigned int u;
  unsigned int b0,b1;
  unsigned char t[128];
  if(!parsehex(&b0)) goto err;
  if(nh==2) {
    if(!parsehex(&b1)) goto err;
  } else b1=-1;
  for(i=0;i<128;i++) {
    if(!parsehex(&u)) break;
    t[i]=u;
    }
  device_sendmessage(cmdport,b0,b1,i,t);
  return 0;
err:
  return 1;
  }
// static int cmd_accelerometer() {
//   int ax,ay,az;
//   accel_getaxyz(&ax,&ay,&az);
//   ostr("Accelerometer: ");
//   osfxp(ax,16,2); osp();
//   osfxp(ay,16,2); osp();
//   osfxp(az,16,2); onl();
//   return 0;
//   }
static void cmd_version() {
  ostr("Firmware version: ");
  ostrnl(FWVERSION);
  }
static void cmd_signature() {
  int i;
  ostr("Firmware signature:");
  for(i=0;i<64;i++) {
    osp();
    o2hex(bootloader_pad[i]);
    }
  onl();
  }

void cmd_prompt() {
  if(!echo) return;
  o1ch('P'); odec(cmdport); o1ch('>');
  }

void proc_cmd() {
  parse_reset();
  for(;;) {
    skspsc();
    if(parse_eol()) goto done;
    else if(strmatch("help"         )) cmd_help();
    else if(strmatch("?"            )) cmd_help();
    else if(strmatch("clear_faults" )) port_clearfaults();
    else if(strmatch("port"         )) { if(cmd_port())          goto err; }
    else if(strmatch("list"         )) { if(cmd_list())          goto err; }
    else if(strmatch("ledmode"      )) { if(cmd_ledmode())       goto err; }
    else if(strmatch("pwm"          )) { if(cmd_pwm())           goto err; }
    else if(strmatch("coast"        )) { if(cmd_coast())         goto err; }
//!!!    else if(strmatch("pwmfreq" )) { if(cmd_pwmfreq())       goto err; }
    else if(strmatch("pid"          )) { if(cmd_pid())           goto err; }
    else if(strmatch("set"          )) { if(cmd_set())           goto err; }
    else if(strmatch("off"          )) { if(cmd_off())           goto err; }
    else if(strmatch("on"           )) { if(cmd_on())            goto err; }
    else if(strmatch("vin"          )) { if(cmd_vin())           goto err; }
    else if(strmatch("bias"         )) { if(cmd_bias())          goto err; }
    else if(strmatch("plimit"       )) { if(cmd_plimit())        goto err; }
    else if(strmatch("select"       )) { if(cmd_select(0))       goto err; }
    else if(strmatch("selonce"      )) { if(cmd_select(1))       goto err; }
    else if(strmatch("combi"        )) { if(cmd_combi())         goto err; }
//    else if(strmatch("accelerometer")) { if(cmd_accelerometer()) goto err; }
    else if(strmatch("write1"       )) { if(cmd_write(1))        goto err; }
    else if(strmatch("write2"       )) { if(cmd_write(2))        goto err; }
    else if(strmatch("echo"         )) { if(cmd_echo())          goto err; }
    else if(strmatch("debug"        )) { if(cmd_debug())         goto err; }
    else if(strmatch("version"      )) cmd_version();
    else if(strmatch("signature"    )) cmd_signature();
//    else if(strmatch("bootloader" )) cmd_bootloader();
    else goto err;
    }
err:
  ostrnl("Error");
done:
  cmd_prompt();
  }
