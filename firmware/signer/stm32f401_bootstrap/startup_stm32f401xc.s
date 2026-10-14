/*
 * Real Cortex-M4 startup file for the STM32F401CCU6 bootstrap target
 * -- the real vector table and reset handler that turn a linked ELF
 * into something that can actually boot. Standard structure (matches
 * what ST's own CubeMX generates) -- not invented from scratch.
 *
 * Real, disclosed scope limit: only the core Cortex-M4 exception
 * vectors are populated below (NMI/HardFault/etc. through SysTick).
 * Real STM32F401 peripheral interrupt vectors (USART1_IRQn, etc.) are
 * NOT listed -- this bootstrap pass runs entirely polled (matching
 * main.c's existing uart_read_byte()/uart_write_byte()), with
 * interrupts never enabled, so this is a real, acceptable
 * simplification for THIS pass, not a claim that interrupt-driven
 * operation would work correctly as-is.
 */
    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

    .global g_pfnVectors
    .global Reset_Handler

    .section .isr_vector,"a",%progbits
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler

    .size g_pfnVectors, . - g_pfnVectors

    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    ldr sp, =_estack

    /* real copy of .data from flash to RAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b LoopCopyDataInit

CopyDataInit:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4
LoopCopyDataInit:
    adds r4, r0, r3
    cmp r4, r1
    bcc CopyDataInit

    /* real zeroing of .bss */
    ldr r2, =_sbss
    ldr r4, =_ebss
    movs r3, #0
    b LoopFillZerobss
FillZerobss:
    str r3, [r2]
    adds r2, r2, #4
LoopFillZerobss:
    cmp r2, r4
    bcc FillZerobss

    bl main
    b .
    .size Reset_Handler, . - Reset_Handler

    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
    b Default_Handler
    .size Default_Handler, . - Default_Handler

    .weak NMI_Handler
    .thumb_set NMI_Handler,Default_Handler
    .weak HardFault_Handler
    .thumb_set HardFault_Handler,Default_Handler
    .weak MemManage_Handler
    .thumb_set MemManage_Handler,Default_Handler
    .weak BusFault_Handler
    .thumb_set BusFault_Handler,Default_Handler
    .weak UsageFault_Handler
    .thumb_set UsageFault_Handler,Default_Handler
    .weak SVC_Handler
    .thumb_set SVC_Handler,Default_Handler
    .weak DebugMon_Handler
    .thumb_set DebugMon_Handler,Default_Handler
    .weak PendSV_Handler
    .thumb_set PendSV_Handler,Default_Handler
    .weak SysTick_Handler
    .thumb_set SysTick_Handler,Default_Handler
