.section .text
.syntax unified
.cpu cortex-m4
.thumb
.extern main
.global _start
.global reboot

.equ ramtop,0x20050000

.org 0
.word ramtop
.word _start+1 @ reset vector +1 for Thumb mode
.word 0        @ other vectors
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0

@ IRQ vectors start here

.word 0 // 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0

.word 0 // 10
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0

.word 0 // 20
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0

.word 0 // 30
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word UART1IRQ+1
.word UART2IRQ+1
.word UART3IRQ+1

.word 0 // 40
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0

.word 0 // 50
.word 0
.word UART4IRQ+1
.word UART5IRQ+1
.word 0
.word Timer7IRQ+1
.word 0
.word 0
.word 0
.word 0

.word 0 // 60
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0

.word 0 // 70
.word UART6IRQ+1
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0

.word 0 // 80
.word 0
.word UART7IRQ+1
.word UART8IRQ+1
.word 0
.word 0
.word 0
.word 0
.word UART9IRQ+1
.word UART10IRQ+1

.word 0 // 90
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0

.word 0 // 100
.word 0

_start:
 ldr r0,=#0xe000ed88                             @ SCB->CPACR
 ldr r1,[r0]
 orr r1,r1,#0x00f00000                           @ enable CP10 and CP11 to allow FPU use: main() may immediately use an FPU instruction
 str r1,[r0]
 bl main
 ldr r0,=#0xe0000000
 bx r0 @ crash qemu

.thumb_func
_reboot:
 ldr r0,=#0x40013800                             @ SYSCFG base
 movs r1,#3
 str r1,[r0]                                     @ remap SRAM to address 0
 dsb
 ldr r0,=#0x00000000
 ldmia r0,{r0,r1} 
 mov r13,r0
 bx r1

.equ reboot,_reboot+0x08000000                   @ pointer to where the function resides in non-remapped flash

.ltorg
