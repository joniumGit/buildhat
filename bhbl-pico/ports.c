#include "control.h"
#include "ioconv.h"
#include "ports.h"
#include "timer.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

typedef unsigned char UC;

struct porthw {
  i2c_inst_t*i2c;                                          // port for communication with motor driver
  int i2c_add;
  } porthw[NPORTS];

struct porthw porthw[NPORTS]={
//     i2c add        
    { i2c0,0x60+(0<<3) },
    { i2c0,0x60+(1<<3) },
    { i2c1,0x60+(2<<3) },
    { i2c1,0x60+(3<<3) }
  };

// attempt to reset the I²C bus: see Analog Devices application note AN-686
static void i2c_reset(i2c_inst_t*i2c) {
  int i,scl,sda;
  ostrnl("Attempting reset of I²C bus");
  i2c_deinit(i2c);
  sda=i2c==i2c0?PIN_I2C0_SDA:PIN_I2C1_SDA;
  scl=i2c==i2c0?PIN_I2C0_SCL:PIN_I2C1_SCL;
  gpio_init(sda); gpio_pull_up(sda);
  gpio_init(scl); gpio_set_dir(scl,1); gpio_put(scl,1); // force clock high
  for(i=0;i<9;i++) {
    wait_ticks(2);
    gpio_put(scl,0); // generate a pulse on SCL
    wait_ticks(2);
    gpio_put(scl,1);
    }
  gpio_set_dir(sda,1); gpio_put(sda,1); // SDA high
  wait_ticks(2);
  gpio_put(scl,0); // generate a rising edge on SCL with SDA high = STOP condition
  wait_ticks(2);
  gpio_put(scl,1);
  i2c_init(i2c,400000);
  gpio_init(sda); gpio_set_function(sda,GPIO_FUNC_I2C); gpio_pull_up(sda);
  gpio_init(scl); gpio_set_function(scl,GPIO_FUNC_I2C); gpio_pull_up(scl);
  wait_ticks(10); // delay before retry
  }

static int port_i2c_write(int p,const uint8_t*src,size_t len,bool nostop) {
  int i,j,u;
  for(j=0;;j++) {   // three tries, then attempt a reset, then one more try
    for(i=0;i<3;i++) { // up to 3 tries the first time
      u=i2c_write_timeout_us(porthw[p].i2c,porthw[p].i2c_add,src,len,nostop,10000); // allow 10ms timeout
      if(u==len) return u;
      if(u==PICO_ERROR_GENERIC) { ostr("<P"); odec(p); ostrnl(": generic I²C bus error>"); }
      if(u==PICO_ERROR_TIMEOUT) { ostr("<P"); odec(p); ostrnl(": I²C bus timeout>"      ); }
      if(j) return u; // give up and return an error code if we have already tried resetting
      wait_ticks(10); // delay before retry
      }
    i2c_reset(porthw[p].i2c);
    }
  }

static int port_i2c_read(int p,uint8_t*dst,size_t len,bool nostop) {
  int i,j,u;
  for(j=0;;j++) {   // three tries, then attempt a reset, then one more try
    for(i=0;i<3;i++) { // up to 3 tries the first time
      u=i2c_read_timeout_us (porthw[p].i2c,porthw[p].i2c_add,dst,len,nostop,10000); // allow 10ms timeout
      if(u==len) return u;
      if(u==PICO_ERROR_GENERIC) { ostr("<P"); odec(p); ostrnl(": generic I²C bus error>"); }
      if(u==PICO_ERROR_TIMEOUT) { ostr("<P"); odec(p); ostrnl(": I²C bus timeout>"      ); }
      if(j) return u; // give up and return an error code if we have already tried resetting
      wait_ticks(10); // delay before retry
      }
    i2c_reset(porthw[p].i2c);
    }
  }

static int port_readi2cbyte(int p,int b) {
  UC t;
  t=b;
  if(port_i2c_write(p,&t,1,0)==-2) return -1;
  if(port_i2c_read (p,&t,1,0)==-2) return -1;
  return t;
  }

static inline void port_setreg(int p,int r,int d) {
  UC t[2]={r,d};
  port_i2c_write(p,t,2,0);
  }

static inline void port_set2reg(int p,int r,int d0,int d1) {
  UC t[3]={r,d0,d1};
  port_i2c_write(p,t,3,0);
  }

void init_ports() {
  int i;

  gpio_init(PIN_LED0      ); gpio_set_dir(PIN_LED0      ,1);
  gpio_init(PIN_LED1      ); gpio_set_dir(PIN_LED1      ,1);
  i2c_init(i2c0,400000);
  i2c_init(i2c1,400000);
  gpio_set_function(PIN_I2C0_SDA,GPIO_FUNC_I2C);
  gpio_set_function(PIN_I2C0_SCL,GPIO_FUNC_I2C);
  gpio_set_function(PIN_I2C1_SDA,GPIO_FUNC_I2C);
  gpio_set_function(PIN_I2C1_SCL,GPIO_FUNC_I2C);
  gpio_pull_up(PIN_I2C0_SDA);
  gpio_pull_up(PIN_I2C0_SCL);
  gpio_pull_up(PIN_I2C1_SDA);
  gpio_pull_up(PIN_I2C1_SCL);
  gpio_put(PIN_LED0,1); // light red LED
  gpio_put(PIN_LED1,0);
//  for(;;) {
//  ostrnl("Checking I²C0:");
//  for(i=0x08;i<0x80;i++) {
//    unsigned char t;
//    if(i%8==0) { o2hex(i); osp(); }
//    if(i2c_read_blocking(i2c0,i,&t,1,0)==-2) o1ch('.'); else o1ch('+'); osp();
//    if(i%8==7) onl();
//    }
//  ostrnl("Checking I²C1:");
//  for(i=0x08;i<0x80;i++) {
//    if(i%8==0) { o2hex(i); osp(); }
//    unsigned char t;
//    if(i2c_read_blocking(i2c1,i,&t,1,0)==-2) o1ch('.'); else o1ch('+'); osp();
//    if(i%8==7) onl();
//    }
//    gpio_put(PIN_LED1,1);
//    wait_ticks(100);
//    gpio_put(PIN_LED1,0);
//    wait_ticks(900);
//    }
  for(i=0;i<NPORTS;i++)
    port_setreg(i,0xf5,0x01);
  }

