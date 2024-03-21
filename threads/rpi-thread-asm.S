/*
 * implement these.  ordered in difficulty.
 */
#include "rpi-asm.h"

@ return the current stack pointer.
MK_FN(rpi_get_sp)
    mov r0, sp
    bx lr

MK_FN(rpi_cswitch)
       
    @ push all registers to the stack
    push {r4, r5, r6, r7, r8, r9, r10, r11, lr} @ push all registers onto the stack
    @ set old saved stack pointer to sp
    str sp, [r0]  
    
    @ make the new saved sp as sp
    mov sp, r1
    @ restore all the registers from the new sp
    pop {r4, r5, r6, r7, r8, r9, r10, r11, lr}
    bx lr

@ [Make sure you can answer: why do we need to do this?]
@
@ use this to setup each thread for the first time.
@ setup the stack so that when cswitch runs it will:
@	- load address of <rpi_init_trampoline> into LR
@	- <code> into r1, 
@	- <arg> into r0
@ 
MK_FN(rpi_init_trampoline)
    mov r1, r4  @ saved after context switch
    mov r0, r5  

    blx r1
    bl rpi_exit @ call to rpi_exit
    bx lr
