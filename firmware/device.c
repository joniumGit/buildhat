#include <string.h>
#include "debug.h"
#include "device.h"
#include "ports.h"
#include "ioconv.h"

struct devinfo devinfo[NPORTS];

void device_init(int dn) {
  struct devinfo*d=devinfo+dn;
  memset(d,0,sizeof(struct devinfo));
  d->type=-1;
  }

void device_dump(int dn) {
  struct devinfo*d=devinfo+dn;
  int i;
  ostr("P"); o1hex(dn); ostr(": ");
  if(d->type==-1) {
    ostrnl("no device detected");
    return;
    }
  ostr("type "); o2hex(d->type); onl();
  ostr("  nmodes ="); odec(d->nmodes ); onl();
  ostr("  nview  ="); odec(d->nview  ); onl();
  ostr("  baud   ="); odec(d->baud   ); onl();
  ostr("  hwver  ="); o8hex(d->hwver  ); onl();
  ostr("  swver  ="); o8hex(d->swver  ); onl();
  for(i=0;i<MAXNMODES;i++) if(d->modes[i].name[0]) {
    ostr("  M"); odec(i); osp(); ostr(d->modes[i].name);
    ostr(" SI = "); ostr(d->modes[i].symbol); onl();
    ostr("    RAW: "); o8hex(d->modes[i].rawl); osp(); o8hex(d->modes[i].rawh);
    ostr("    PCT: "); o8hex(d->modes[i].pctl); osp(); o8hex(d->modes[i].pcth);
    ostr("    SI: " ); o8hex(d->modes[i].sil ); osp(); o8hex(d->modes[i].sih );
    onl();
    }
  ostr("     speed PID:"); for(i=0;i<4;i++) { osp(); o8hex(d->speedpid[i]); } onl();
  ostr("  position PID:"); for(i=0;i<4;i++) { osp(); o8hex(d->  pospid[i]); } onl();
  }
