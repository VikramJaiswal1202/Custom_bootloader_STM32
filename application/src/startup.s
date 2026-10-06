.syntax unified
.cpu cortex-m4
.thumb

.section .isr_vector, "a", %progbits

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


.section .text.Reset_Handler, "ax", %progbits

.global Reset_Handler
.type Reset_Handler, %function

Reset_Handler:

    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata

.data_copy_loop:
    cmp r1, r2
    bhs .data_copy_done

    ldr r3, [r0]
    str r3, [r1]

    adds r0, r0, #4
    adds r1, r1, #4

    b .data_copy_loop

.data_copy_done:

    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0

.bss_clear_loop:
    cmp r1, r2
    bhs .bss_clear_done

    str r3, [r1]
    adds r1, r1, #4

    b .bss_clear_loop

.bss_clear_done:

    bl main

.main_return:
    b .main_return

.size Reset_Handler, .-Reset_Handler


.section .text.Default_Handler, "ax", %progbits

.global Default_Handler
.type Default_Handler, %function

Default_Handler:
    b Default_Handler

.size Default_Handler, .-Default_Handler


.global NMI_Handler
.global HardFault_Handler
.global MemManage_Handler
.global BusFault_Handler
.global UsageFault_Handler
.global SVC_Handler
.global DebugMon_Handler
.global PendSV_Handler
.global SysTick_Handler

.set NMI_Handler,        Default_Handler
.set HardFault_Handler,  Default_Handler
.set MemManage_Handler,  Default_Handler
.set BusFault_Handler,   Default_Handler
.set UsageFault_Handler, Default_Handler
.set SVC_Handler,        Default_Handler
.set DebugMon_Handler,   Default_Handler
.set PendSV_Handler,     Default_Handler
.set SysTick_Handler,    Default_Handler