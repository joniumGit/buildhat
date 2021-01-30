#include "stm32f413xx.h"
#include <string.h>
#include "common.h"
#include "debug.h"
#include "system.h"
#include "gpio.h"
#include "ioconv.h"
#include "hardware.h"
#include "control.h"

void port_uart_irq() { }

#define TIMEOUT 1000                             // in milliseconds

volatile unsigned int tick;
unsigned int tick0;

#define IMAGEBUF ((unsigned char*)(0x20000000))  // SRAM1 block always mapped at this address
#define IMAGEBUFSIZE (256*1024)                  // SRAM1 is 256K
unsigned int image_size=0;

#define STX 0x02                                 // ASCII control codes
#define ETX 0x03

void Timer7IRQ() {
  TIM7->SR=0;
  tick++;
  }

void init_timer() {
  RCC->APB1ENR|=RCC_APB1ENR_TIM7EN;              // enable clock for TIM7 (main tick)
  TIM7->PSC=99;                                  // clock at 1MHz
  TIM7->ARR=999;                                 // 1kHz tick
  TIM7->CR1=1;                                   // enable timer
  TIM7->DIER=1;
  TIM7->SR=0;
  tick=0;
  nvic_intena(TIM7_IRQn);                        // enable TIM7 interrupt
  }

//// wait for a time period between n-1 and n ticks, n>0
//static void wait_ticks(unsigned int n) {
//  n+=tick;
//  while(tick!=n) __WFI();
//  }

static void reset_timeout() { tick0=tick; }
static int timeout() { return (int)(tick-tick0)>TIMEOUT; }


static int verify(UC*buf,int len) {
  return 1;
  }

unsigned int checksum(unsigned char*p,int l) {
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
  ostrnl("Commands available:");
  ostrnl("  help, ?            : display this text");
  ostrnl("  load <length>      : initiate upload");
  ostrnl("                       followed by <STX><<length> data bytes><ETX>");
  ostrnl("  clear              : clear any uploaded image");
  ostrnl("  verify             : verify upload");
  ostrnl("  reboot             : reboot if upload successfully verified");
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
  return 0;
  }

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
    else goto err;
    }
err:
  ostrnl("Error");
done:
  cmd_prompt();
  }

// =================================== main() ==================================

int main() {
  init_data_bss();
  init_sys();
  init_gpio();
  init_control();
  for(;;)
    proc_ctrl();
  return 0;
  }
