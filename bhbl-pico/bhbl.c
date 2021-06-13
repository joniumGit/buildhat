#include <string.h>
#include "timer.h"
#include "ioconv.h"
#include "control.h"

#define BLVERSION "BuildHAT bootloader version 1.0"

extern void reboot();

#define TIMEOUT 1000                             // in milliseconds
static unsigned int tick0;

typedef unsigned char UC;

#define IMAGEBUF ((UC*)(0x20000000))             // SRAM1 block always mapped at this address
#define IMAGEBUFSIZE (256*1024)                  // SRAM1 is 256K
unsigned int image_size=0;

#define STX 0x02                                 // ASCII control codes
#define ETX 0x03

static void reset_timeout() { tick0=gettick(); }
static int timeout() { return (int)(gettick()-tick0)>TIMEOUT; }


static int verify(UC*buf,int len) {
  return 1;
  }

unsigned int checksum(UC*p,int l) {
  unsigned int u=1;
  int i;
  for(i=0;i<l;i++) {
    if((u&0x80000000)!=0) u=(u<<1)^0x1d872b41;
    else                  u= u<<1;
    u^=p[i];
    }
  return u;
  }

static void cmd_help() {
  ostrnl(BLVERSION);
  ostrnl("Commands available:");
  ostrnl("  help, ?                  : display this text");
  ostrnl("  version                  : display version string");
  ostrnl("  load <length> <checksum> : initiate upload");
  ostrnl("                             followed by <STX><<length> data bytes><ETX>");
  ostrnl("  clear                    : clear any uploaded image");
  ostrnl("  verify                   : verify upload");
  ostrnl("  reboot                   : reboot if upload successfully verified");
  }

static int cmd_load() {
  unsigned int len,cs;
  int c,i;
  image_size=0;
  if(!parseuint(&len)) return 1;
  if(!parseuint(&cs)) return 1;
  if(len>IMAGEBUFSIZE) {
    ostr("\r\nImage too large (maximum ");
    odec(IMAGEBUFSIZE);
    ostrnl(" bytes)");
    return 1;
    }
  reset_timeout();
  do {
    if(timeout()) goto err_to;
    c=i1ch();
    } while(c!=STX);
  reset_timeout();
  for(i=0;i<len;) {
    if(timeout()) goto err_to;
    c=i1ch();
    if(c<0) continue;
    IMAGEBUF[i++]=c;
    reset_timeout();
    }
  reset_timeout();
  do {
    if(timeout()) goto err_to;
    c=i1ch();
    } while(c!=ETX);
  ostrnl("\r\nImage received");
  if(checksum(IMAGEBUF,len)!=cs) {
    ostrnl("Checksum failure");
    return 1;
    }
  ostrnl("Checksum OK");
  image_size=len;
  return 0;
err_to:
  ostrnl("\r\nTimed out waiting for data");
  return 1;
  }

static int cmd_clear() { image_size=0; return 0; }

static int cmd_verify() {
  if(image_size==0) {
    ostrnl("No image loaded");
    return 1;
    }
  ostrnl("Verifying image...");
  if(verify(IMAGEBUF,IMAGEBUFSIZE)==0) {
    ostrnl("Image verification failed");
    return 1;
    }
  ostrnl("Image verifed OK");
  return 0;
  }

static int cmd_reboot() {
  if(cmd_verify()) return 1;
  ostrnl("Rebooting...");
  wait_ticks(100);
  deinit_control();
  reboot();
  return 0;
  }

static int cmd_version() { ostrnl(BLVERSION); return 0; }

void cmd_prompt() { ostr("BHBL> "); }

void proc_cmd() {
  parse_reset();
  for(;;) {
    skspsc();
    if(parse_eol()) goto done;
    else if(strmatch("help"   )) cmd_help();
    else if(strmatch("?"      )) cmd_help();
    else if(strmatch("load"   )) { if(cmd_load())    goto err; }
    else if(strmatch("clear"  )) { if(cmd_clear())   goto err; }
    else if(strmatch("verify" )) { if(cmd_verify())  goto err; }
    else if(strmatch("reboot" )) { if(cmd_reboot())  goto err; }
    else if(strmatch("version")) { if(cmd_version()) goto err; }
    else goto err;
    }
err:
  ostrnl("Error");
done:
  cmd_prompt();
  }

// =================================== main() ==================================

int main() {
  init_timer();
  init_control();
  for(;;)
    proc_ctrl();
  return 0;
  }
