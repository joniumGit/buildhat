#include "common.h"
#include "hardware.h"
#include "ioconv.h"
#include "pwm_pid.h"
#include "ports.h"
#include "device.h"
#include "debug.h"
#include "control.h"

static const float sintab[256]={
   0.00000, 0.02454, 0.04907, 0.07356, 0.09802, 0.12241, 0.14673, 0.17096,
   0.19509, 0.21910, 0.24298, 0.26671, 0.29028, 0.31368, 0.33689, 0.35990,
   0.38268, 0.40524, 0.42756, 0.44961, 0.47140, 0.49290, 0.51410, 0.53500,
   0.55557, 0.57581, 0.59570, 0.61523, 0.63439, 0.65317, 0.67156, 0.68954,
   0.70711, 0.72425, 0.74095, 0.75721, 0.77301, 0.78835, 0.80321, 0.81758,
   0.83147, 0.84485, 0.85773, 0.87009, 0.88192, 0.89322, 0.90399, 0.91421,
   0.92388, 0.93299, 0.94154, 0.94953, 0.95694, 0.96378, 0.97003, 0.97570,
   0.98079, 0.98528, 0.98918, 0.99248, 0.99518, 0.99729, 0.99880, 0.99970,
   1.00000, 0.99970, 0.99880, 0.99729, 0.99518, 0.99248, 0.98918, 0.98528,
   0.98079, 0.97570, 0.97003, 0.96378, 0.95694, 0.94953, 0.94154, 0.93299,
   0.92388, 0.91421, 0.90399, 0.89322, 0.88192, 0.87009, 0.85773, 0.84485,
   0.83147, 0.81758, 0.80321, 0.78835, 0.77301, 0.75721, 0.74095, 0.72425,
   0.70711, 0.68954, 0.67156, 0.65317, 0.63439, 0.61523, 0.59570, 0.57581,
   0.55557, 0.53500, 0.51410, 0.49290, 0.47140, 0.44961, 0.42756, 0.40524,
   0.38268, 0.35990, 0.33689, 0.31368, 0.29028, 0.26671, 0.24298, 0.21910,
   0.19509, 0.17096, 0.14673, 0.12241, 0.09802, 0.07356, 0.04907, 0.02454,
   0.00000,-0.02454,-0.04907,-0.07356,-0.09802,-0.12241,-0.14673,-0.17096,
  -0.19509,-0.21910,-0.24298,-0.26671,-0.29028,-0.31368,-0.33689,-0.35990,
  -0.38268,-0.40524,-0.42756,-0.44961,-0.47140,-0.49290,-0.51410,-0.53500,
  -0.55557,-0.57581,-0.59570,-0.61523,-0.63439,-0.65317,-0.67156,-0.68954,
  -0.70711,-0.72425,-0.74095,-0.75721,-0.77301,-0.78835,-0.80321,-0.81758,
  -0.83147,-0.84485,-0.85773,-0.87009,-0.88192,-0.89322,-0.90399,-0.91421,
  -0.92388,-0.93299,-0.94154,-0.94953,-0.95694,-0.96378,-0.97003,-0.97570,
  -0.98079,-0.98528,-0.98918,-0.99248,-0.99518,-0.99729,-0.99880,-0.99970,
  -1.00000,-0.99970,-0.99880,-0.99729,-0.99518,-0.99248,-0.98918,-0.98528,
  -0.98079,-0.97570,-0.97003,-0.96378,-0.95694,-0.94953,-0.94154,-0.93299,
  -0.92388,-0.91421,-0.90399,-0.89322,-0.88192,-0.87009,-0.85773,-0.84485,
  -0.83147,-0.81758,-0.80321,-0.78835,-0.77301,-0.75721,-0.74095,-0.72425,
  -0.70711,-0.68954,-0.67156,-0.65317,-0.63439,-0.61523,-0.59570,-0.57581,
  -0.55557,-0.53500,-0.51410,-0.49290,-0.47140,-0.44961,-0.42756,-0.40524,
  -0.38268,-0.35990,-0.33689,-0.31368,-0.29028,-0.26671,-0.24298,-0.21910,
  -0.19509,-0.17096,-0.14673,-0.12241,-0.09802,-0.07356,-0.04907,-0.02454
  };

static void getsetpoint(int pn) {
  struct portinfo*p=portinfo+pn;
  float u;
  int i;

  u=p->spwavephaseacc+(PWM_UPDATE/1000.0);
  if(u>=p->spwaveperiod) {
    u-=p->spwaveperiod;
    if(p->spwaveshape==WAVE_PULSE||
       p->spwaveshape==WAVE_RAMP)  {               // end of a pulse/ramp?
      o1ch('P'); o1hex(pn);
      if(p->spwaveshape==WAVE_PULSE) ostrnl(": pulse done");
      else                           ostrnl(": ramp done");
      p->spwaveshape =WAVE_SQUARE;                 // return to constant level
      p->spwavemin   =p->spwavemax;
      p->spwaveperiod=1;
      p->spwavephase =0;
      }
    }
  if(u>=p->spwaveperiod) u=0;
  p->spwavephaseacc=u;
  u/=p->spwaveperiod;
  u+=p->spwavephase;
  if(u>=1) u-=1;
  switch(p->spwaveshape) {
default:
case WAVE_SQUARE:
    if(u>=0.5) u=p->spwavemax;
    else       u=p->spwavemin;
    break;
case WAVE_SINE:
    i=((int)(u*256+0.5))%256;
    u=p->spwavemin+(p->spwavemax-p->spwavemin)*(sintab[i]+1)*0.5;
    break;
case WAVE_TRI:
    if(u>=0.5) u=1-u;
    u=p->spwavemin+(p->spwavemax-p->spwavemin)*u*2;
    break;
case WAVE_PULSE:
    u=p->spwavemin;
    break;
case WAVE_RAMP:
    u=p->spwavemin+(p->spwavemax-p->spwavemin)*u;
    break;
    }
  p->setpoint=u;
  }

void proc_pwm(int pn) {
  struct portinfo*p=portinfo+pn;
  struct devinfo*d=devinfo+pn;
  float u;
  float err,derr;

  getsetpoint(pn);
  if(!d->connected) {
    port_set_pwm(pn,0);
    return;
    }

  if(p->coast!=0) {                                // coasting?
    port_motor_coast(pn);
    return;
    }
  if(p->pwmmode==0) {                              // direct PWM?
    port_set_pwm(pn,p->setpoint);
    return;
    }

// here we are in PID controller mode: first try to extract the process variable
  if(device_varfrommode(p->pvport,p->pvmode,p->pvoffset,p->pvformat,
                           p->pvscale,p->pvunwrap,&p->pid_pv_last,&p->pid_pv)==0) return;

DEB_PID { o1ch('P'); o1hex(pn); ostr(": pv="); ostr(sfloat(p->pid_pv)); ostr(" sp="); ostr(sfloat(p->setpoint)); }
  err=p->pid_pv-p->setpoint;
  if(err<0.01&&err>-0.01) err=0;                   // dead zone
  derr=(err-p->pid_perr)/(PWM_UPDATE/1000.0);      // derivative error
  p->pid_perr=err;
  p->pid_ierr+=err*(PWM_UPDATE/1000.0);            // integral error
  CLAMP(p->pid_ierr,-p->windup,p->windup);
DEB_PID { ostr(": e="); ostr(sfloat(err)); ostr(" ie="); ostr(sfloat(p->pid_ierr)); ostr(" de="); ostr(sfloat(derr)); }
  u=-(err*p->Kp+p->pid_ierr*p->Ki+derr*p->Kd);
DEB_PID { ostr(" cv="); ostr(sfloat(u)); onl(); }
  port_set_pwm(pn,u);
  return;
  }
