#include <string.h>
#include "debug.h"
#include "device.h"
#include "ports.h"
#include "control.h"
#include "ioconv.h"

struct devinfo devinfo[NPORTS];

void device_init(int dn) {
  struct devinfo*d=devinfo+dn;
  memset(d,0,sizeof(struct devinfo));
  d->type=-1;
  }

void device_dump(int dn) {
  struct devinfo*d=devinfo+dn;
  int i,j;
  ostr("P"); o1hex(dn); ostr(": ");
  if(d->connected&&d->id>0&&d->id<12) {
    ostr("connected to passive ID ");
    odec(d->id);
    onl();
    return;
    }
  if(d->type==-1) {
    ostrnl("no device detected");
    return;
    }
  ostr("connected to active ID "); o2hex(d->type); onl();
  ostr("type "); o2hex(d->type); onl();
  ostr("  nmodes ="); odec(d->nmodes ); onl();
  ostr("  nview  ="); odec(d->nview  ); onl();
  ostr("  baud   ="); odec(d->baud   ); onl();
  ostr("  hwver  ="); o8hex(d->hwver  ); onl();
  ostr("  swver  ="); o8hex(d->swver  ); onl();
  for(i=0;i<MAXNMODES;i++) if(d->modes[i].name[0]) {
    ostr("  M"); odec(i); osp(); ostr(d->modes[i].name);
    ostr(" SI = "); ostr(d->modes[i].symbol); onl();
    ostr("    format count="); odec(d->modes[i].format_count);
              ostr( " type="); odec(d->modes[i].format_type);
              ostr(" chars="); odec(d->modes[i].format_chars);
              ostr(   " dp="); odec(d->modes[i].format_dp); onl();
    ostr("    RAW: "); o8hex(d->modes[i].rawl); osp(); o8hex(d->modes[i].rawh);
    ostr("    PCT: "); o8hex(d->modes[i].pctl); osp(); o8hex(d->modes[i].pcth);
    ostr("    SI: " ); o8hex(d->modes[i].sil ); osp(); o8hex(d->modes[i].sih );
    onl();
    }
  for(i=0;i<d->ncombis;i++) {
    unsigned short u=d->validcombis[i];
    ostr("  C"); odec(i); ostr(": ");
    for(j=0;j<16;j++) {
      if(u&1) {
        o1ch('M');
        odec(j);
        if(u!=1) o1ch('+');
        }
      u>>=1;
      }
    onl();
    }
  ostr("     speed PID:"); for(i=0;i<4;i++) { osp(); o8hex(d->speedpid[i]); } onl();
  ostr("  position PID:"); for(i=0;i<4;i++) { osp(); o8hex(d->  pospid[i]); } onl();
  }

int device_varfrommode(int port,int mode,int offset,int format,float*var) {
  char buf[4];
  int i;
  float v;
  struct devinfo*dvp;

  if(port<0||port>=NPORTS) return 0;
  dvp=devinfo+port;
  if(dvp->modedatalen[mode]==0) return 0;
  for(i=0;i<(format&0x0f);i++) buf[i]=dvp->modedata[mode][offset+i]; // so that the value is aligned
  switch(format) {
case 0x001: v=(float)*(unsigned char* )buf; break;
case 0x101: v=(float)*(  signed char* )buf; break;
case 0x002: v=(float)*(unsigned short*)buf; break;
case 0x102: v=(float)*(  signed short*)buf; break;
case 0x004: v=(float)*(unsigned int*  )buf; break;
case 0x104: v=(float)*(  signed int*  )buf; break;
case 0x204: v=       *(         float*)buf; break;
default:    v=0;
    }
  *var=v;
  return 1;
  }

void device_dumpmodevar(int port,int mode,int offset,int format) {
  float v;
  if(device_varfrommode(port,mode,offset,format,&v)==0) return;
  o1ch('P'); o1hex(port); ostr("V: "); ostrnl(sfloat(v));
  }

void device_dumpmoderaw(int port,int mode) {
  struct devinfo*dvp;
  int i;

  if(port<0||port>=NPORTS) return;
  dvp=devinfo+port;
  if(dvp->modedatalen[mode]==0) return;
  o1ch('P'); o1hex(port); ostr("R:");
  for(i=0;i<dvp->modedatalen[mode];i++) {
    osp(); o2hex(dvp->modedata[mode][i]);
    }
  onl();
  }

static int formattolen(int f) {
  switch(f) {
case 0: return 1;
case 1: return 2;
case 2: return 4;
case 3: return 4;
    }
  return 1;
  }

static void dumpfmt(UC*p,struct modeinfo*mp) {
  UC b[4];
  switch(mp->format_type) {
case 0: osdec((signed char )(p[0])); break;
case 1: osdec((signed short)(p[0]+(p[1]<<8))); break;
case 2: osdec((signed int  )(p[0]+(p[1]<<8)+(p[2]<<16)+(p[3]<<24))); break;
case 3:
    memcpy(b,p,4);
    ofloat(*(float*)b);
    break;
    }
  }

void device_dumpmodefmt(int port,int mode) {
  struct devinfo*dvp;
  struct modeinfo*mp;
  int i,j,k,l;

  if(port<0||port>=NPORTS) return;
  dvp=devinfo+port;
  mp=&(dvp->modes[mode]);

  if(mp->combi_count==0) {                                 //  a normal mode
    if(dvp->modedatalen[mode]==0) return;
    o1ch('P'); o1hex(port); o1ch('M'); odec(mode); ostr(":");
    l=formattolen(mp->format_type);                        // number of bytes in this format
    for(i=0,j=0;i<=dvp->modedatalen[mode]-l&&j<mp->format_count;i+=l,j++) {
      osp();
      dumpfmt(&(dvp->modedata[mode][i]),mp);
      }
  } else {
    o1ch('P'); o1hex(port); o1ch('C'); odec(mode); ostr(":");
    for(i=0,k=0;i<mp->combi_count;i++) {
      j=mp->combi_mode[i];
      l=formattolen(dvp->modes[j].format_type);
      osp();
      dumpfmt(&(dvp->modedata[mode][k]),&dvp->modes[j]);
      k+=l;
      }
    }
  onl();
  if(portinfo[port].selmodeonce==1) {
    portinfo[port].selmodeonce=0;
    portinfo[port].selmode=-1;
    portinfo[port].selrxcount=0;
    }
  }
