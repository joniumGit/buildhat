.section .text
.syntax unified
.cpu cortex-m0
.thumb
.global reboot

.equ RAMVECS,0x20000100
.equ VTOR,0xe000ed08

.thumb_func
reboot:
 ldr r0,=#RAMVECS
 ldr r1,=#VTOR
 str r0,[r1]
 ldmia r0,{r0,r1} 
 msr msp,r0
 bx r1
.ltorg
