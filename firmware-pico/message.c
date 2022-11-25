#include <string.h>
#include "common.h"
#include "hardware.h"
#include "ioconv.h"
#include "debug.h"
#include "control.h"
#include "ports.h"
#include "device.h"

// ========================= MESSAGE PROCESSING: TRANSMIT ======================

// send a single-byte "system" message
void device_sendsys(int dn,unsigned char b) {
  port_sendmessage(dn,&b,1);
  }

// send a message with a payload
// b0 and b1 are first and second bytes of message
// if b1==-1 it is not sent
// plen is payload length, 0..128
void device_sendmessage(int dn,int b0,int b1,int plen,unsigned char*payload) {
  unsigned char buf[TXBLEN];
  int i,j,c;
  int lplen;                     // log₂ of payload length
  CLAMP(plen,0,128);
  for(lplen=0;lplen<7;lplen++) if(1<<lplen>=plen) break;
  j=0;
  c=buf[j++]=(b0&0xc7)|(lplen<<3);
  if(b1>=0) c^=buf[j++]=b1;
  for(i=0;i<plen    ;i++) c^=buf[j++]=payload[i];
  for(   ;i<1<<lplen;i++) c^=buf[j++]=0;           // pad with zeros
  buf[j++]=c^0xff;                                 // insert checksum
//  for(i=0;i<j;i++) { osp(); o2hex(buf[i]); } onl();
  port_sendmessage(dn,buf,j);
  }

// convenience functions to send 1-, 2- and 4-byte messages
void device_sendchar (int dn,int b0,int b1,unsigned char  v) { device_sendmessage(dn,b0,b1,1,(unsigned char*)&v); }
void device_sendshort(int dn,int b0,int b1,unsigned short v) { device_sendmessage(dn,b0,b1,2,(unsigned char*)&v); }
void device_sendint  (int dn,int b0,int b1,unsigned int   v) { device_sendmessage(dn,b0,b1,4,(unsigned char*)&v); }

// send a 2-part SELECT message that works with extended modes
void device_sendselect(int d,int m) {
  ostrnl("D_SS");
  device_sendchar(d,0x46,-1,m&0x8);                // MODESET message !!! strictly we should do outgoing message queueing
  device_sendchar(d,0x43,-1,m&0x7);                // SELECT message
  }

// ========================= MESSAGE PROCESSING: RECEIVE =======================

// process an outstanding message during the setup phase
// return type of message processed if any, else -1
int proc_setupmessages(int pn) {
  struct message*m;
  struct devinfo*d=devinfo+pn;
  struct modeinfo*md;
  int i;
  if(mqhead[pn]==mqtail[pn]) return -1;            // nothing to do?
  m=(struct message*)mqueue[pn]+mqtail[pn];                         // get message
  switch(m->type) {
case 0x04:                                         // ACK
    break;
case 0x40:                                         // TYPE
    d->type=m->payload[0];
    break;
case 0x41:                                         // MODES
    d->nmodes=m->payload[0]&0x07;
    d->nview =m->payload[1]&0x07;
    if(m->plen>=4) {                               // "extended" MODES
      d->nmodes=m->payload[2]&0x0f;
      d->nview =m->payload[3]&0x0f;
      }
    break;
case 0x42:
    if(m->plen>=4) d->baud=*(int*)&m->payload[0];
    break;
case 0x47:
    if(m->plen>=4) d->hwver=*(int*)&m->payload[0];
    if(m->plen>=8) d->swver=*(int*)&m->payload[4];
    break;
case 0x80:
    md=d->modes+m->mode;
    switch(m->cmd&0xdf) {                          // mask out extended mode bit
  case 0x00:
      memcpy(md->name,m->payload,m->plen);
      md->name[m->plen]=0;                         // ensure 0-termination
      break;
  case 0x01:
      if(m->plen>=4) memcpy(&md->rawl,m->payload+0,4);
      if(m->plen>=8) memcpy(&md->rawh,m->payload+4,4);
      break;
  case 0x02:
      if(m->plen>=4) memcpy(&md->pctl,m->payload+0,4);
      if(m->plen>=8) memcpy(&md->pcth,m->payload+4,4);
      break;
  case 0x03:
      if(m->plen>=4) memcpy(&md->sil ,m->payload+0,4);
      if(m->plen>=8) memcpy(&md->sih ,m->payload+4,4);
      break;
  case 0x04:
      if(m->plen<=8) {
        memcpy(md->symbol,m->payload,m->plen);
        md->symbol[m->plen]=0;                     // ensure 0-termination
        }
      break;
  case 0x05:
      if(m->plen>=2) md->prefmap[0]=m->payload[0],md->prefmap[1]=m->payload[1];
      break;
  case 0x06:                                       // allowable COMBI modes
      d->ncombis=0;
      for(i=0;i<m->plen;i+=2) {
        unsigned short u=*(unsigned short*)(&m->payload[i]);
        if(u==0) break;
        d->validcombis[d->ncombis++]=u;
        }
      break;
  case 0x09:                                       // SPID  (device speed pid constants)
      if(m->plen!=16) goto unhandled;
      memcpy((void*)d->speedpid,m->payload,16);
      break;
  case 0x0A:                                       // PPID  (device position pid constants)
      if(m->plen!=16) goto unhandled;
      memcpy((void*)d->pospid,m->payload,16);
      break;
  case 0x80:
      if(m->plen>=2) {
        md->format_count  =m->payload[0]&0x3f;
        md->format_type   =m->payload[1]&0x03;
        if(m->plen>=4) {
          md->format_chars=m->payload[2]&0x0f;
          md->format_dp   =m->payload[3]&0x0f;
        } else {
          md->format_chars=4;
          md->format_dp   =0;
          }
        }
      break;

// unhandled INFO messages

  case 0x07:
      // !!! BIAS  (used in City Train)
  case 0x08:
      // !!! UKEY  (device unique key (serial))
  case 0x0B:
      // !!! PMGMT  (device power management constants)
  case 0x0C:
      // !!! VBIAS  (device voltage bias constants)
  case 0xFF:
      // !!! gets all sensor info
  default:
unhandled:
DEB_UKM {
        o1ch('P'); o1hex(pn);
        ostr(": type=80 cmd="); o2hex(m->cmd);
        ostr("  payload="); o2hexdump(m->payload,m->plen); onl();
        }
      break;
      }
    break;
default:
DEB_UKM {
      o1ch('P'); o1hex(pn); ostr(" message type="); o2hex(m->type); onl();
      }
    break;
    }
  mqtail[pn]=(mqtail[pn]+1)%MQLEN;
  return m->type;
  }

// return type of message processed if any, else -1
int proc_datamessages(int pn) {
  struct message*m;
  struct devinfo*d=devinfo+pn;
  if(mqhead[pn]==mqtail[pn]) return -1;
  m=(struct message*)mqueue[pn]+mqtail[pn];
  switch(m->type) {
case 0xc0:                                         // "data message"
DEB_DPY {
      o1ch('P'); o1hex(pn);
      ostr(": DATA mode="); o1hex(m->mode);
      ostr(" payload="); o2hexdump(m->payload,m->plen); onl();
      }
    memcpy(d->modedata[m->mode],m->payload,m->plen);
    d->modedatalen[m->mode]=m->plen;
    if(m->mode==portinfo[pn].selmode) {
      portinfo[pn].selrxcount++;
      portinfo[pn].selrxever=1;
      }
    break;
default:
DEB_UKM {
      o1ch('P'); o1hex(pn);
      ostr(": data-phase message type ");o2hex(m->type);
      ostr(" payload="); o2hexdump(m->payload,m->plen); onl();
      }
    break;
    }
  mqtail[pn]=(mqtail[pn]+1)%MQLEN;
  return m->type;
  }
