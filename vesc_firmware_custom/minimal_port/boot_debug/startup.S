.syntax unified
.cpu cortex-m4
.thumb
.section .isr_vector,"a",%progbits
.balign 512
.global g_vectors
g_vectors:
    .word _estack
    .word Reset_Handler
    /* Core exceptions + all 82 STM32F405 external interrupt slots. */
    .rept 96
    .word Default_Handler
    .endr

.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.type Reset_Handler,%function
.thumb_func
Reset_Handler:
    cpsid i
    ldr sp, =_estack
    /* Gate latches/mux are made safe before .data/.bss initialization. */
    bl board_gate_safe_early
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:  cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:  ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:  cmp r1, r2
    bcs 4f
    str r3, [r1], #4
    b 3b
4:  bl main
    b Default_Handler
.size Reset_Handler, .-Reset_Handler
.section .note.GNU-stack,"",%progbits
