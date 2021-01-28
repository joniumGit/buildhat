#include "hardware.h"

struct porthw porthw[NPORTS]={
//    en       rts      d5       d6       rx       tx       uart  afn in1      in2      tim  afn c1             c2
    { GPIOA,10,GPIOA,15,GPIOD, 7,GPIOD, 8,GPIOE, 7,GPIOE, 8,UART7, 8, GPIOE, 9,GPIOE,11,TIM1, 1, &(TIM1->CCR1), &(TIM1->CCR2) },
    { GPIOA, 8,GPIOD,13,GPIOD, 9,GPIOD,10,GPIOD, 0,GPIOD, 1,UART4,11, GPIOE,13,GPIOE,14,TIM1, 1, &(TIM1->CCR3), &(TIM1->CCR4) },
    { GPIOE, 5,GPIOD,12,GPIOD,11,GPIOE, 4,GPIOE, 0,GPIOE, 1,UART8, 8, GPIOB, 6,GPIOB, 7,TIM4, 2, &(TIM4->CCR1), &(TIM4->CCR2) },
    { GPIOB, 2,GPIOA, 3,GPIOC,15,GPIOC,14,GPIOD, 2,GPIOC,12,UART5, 8, GPIOB, 8,GPIOB, 9,TIM4, 2, &(TIM4->CCR3), &(TIM4->CCR4) }
  };

// UART ISR dispatch
extern void control_uart_irq();
extern void port_uart_irq(int n);

void  UART1IRQ() {}
void  UART2IRQ() { control_uart_irq(); }
void  UART3IRQ() {}
void  UART4IRQ() { port_uart_irq(1); }
void  UART5IRQ() { port_uart_irq(3); }
void  UART6IRQ() {}
void  UART7IRQ() { port_uart_irq(0); }
void  UART8IRQ() { port_uart_irq(2); }
void  UART9IRQ() {}
void UART10IRQ() {}
