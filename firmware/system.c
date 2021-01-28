#include "common.h"

// initialise the PLL and enable GPIO ports
void init_sys() {
  FLASH->ACR=0x0703;                               // 3 wait states, caches and prefetch enabled
  RCC->CFGR=0x9000;                                // no MCO, no RTC, APB2=50MHz, APB1=50MHz, AHB=100MHz, use HSI for system clock
  RCC->CR=0x10001;                                 // HSI and HSE on
  while((RCC->CR&0x20000)==0) ;                    // wait for HSE ready
  RCC->PLLCFGR=0x24400000+(8<<0)+(100<<6)+(0<<16); // PLL from HSE, M=8, N=100, P=0: f_VCOin=2MHz, f_VCOout=200MHz, sysclk=100MHz
  RCC->CR|=0x1000000;                              // PLL on
  while((RCC->CR&0x2000000)==0) ;                  // wait for PLL ready
  RCC->CFGR|=2;                                    // switch to PLL
  }

void nvic_intena(int n) {                          // enable interrupt n in NVIC
  NVIC->ISER[n/32]=1<<(n%32);
  }

// C startup code to clear BSS section and initialise DATA section
// the linker script ensures everything is aligned to 4-byte boundaries
extern unsigned int _bss_start,_bss_end,_data_start,_data_end,_idata;
void init_data_bss() {
  unsigned int*p,*q;
  for(p=&_bss_start           ;p<&_bss_end ;) *p++=0;
  for(p=&_data_start,q=&_idata;p<&_data_end;) *p++=*q++;
  }
