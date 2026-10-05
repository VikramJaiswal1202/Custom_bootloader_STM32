.syntax unified
.cpu cortex-m4
.thumb


/* ============================================================
 * INTERRUPT VECTOR TABLE
 * ============================================================
 *
 * The Cortex-M4 reads the first two words from the vector table
 * after reset:
 *
 *   0x08000000 -> Initial Main Stack Pointer (MSP)
 *   0x08000004 -> Reset_Handler address
 *
 * The remaining entries contain exception handler addresses.
 *
 * The linker script places .isr_vector at the beginning of
 * the bootloader Flash region:
 *
 *   0x08000000
 * ============================================================ */

.section .isr_vector, "a", %progbits

/* Initial Main Stack Pointer */
.word _estack

/* Reset handler */
.word Reset_Handler


/* ------------------------------------------------------------
 * Cortex-M4 system exception handlers
 * ------------------------------------------------------------ */

.word NMI_Handler
.word HardFault_Handler
.word MemManage_Handler
.word BusFault_Handler
.word UsageFault_Handler

/* Reserved */
.word 0
.word 0
.word 0
.word 0

.word SVC_Handler
.word DebugMon_Handler

/* Reserved */
.word 0

.word PendSV_Handler
.word SysTick_Handler



/* ============================================================
 * RESET HANDLER
 * ============================================================
 *
 * Execution begins here after reset.
 *
 * Responsibilities:
 *
 *   1. Copy initialized .data from Flash to RAM
 *   2. Clear .bss in RAM
 *   3. Call main()
 *   4. Stay in an infinite loop if main() returns
 * ============================================================ */

.section .text.Reset_Handler, "ax", %progbits

.global Reset_Handler
.type Reset_Handler, %function

Reset_Handler:

    /* ========================================================
     * 1. COPY .data FROM FLASH TO RAM
     * ========================================================
     *
     * The linker script provides:
     *
     *   _sidata = Flash address of initial .data values
     *   _sdata  = RAM start of .data
     *   _edata  = RAM end of .data
     *
     * Registers:
     *
     *   r0 = source address
     *   r1 = destination address
     *   r2 = destination end address
     * ======================================================== */

    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata


.data_copy_loop:

    /* Have we reached the end of .data? */
    cmp r1, r2

    /* Yes -> continue with .bss */
    bhs .data_copy_done

    /* Load one 32-bit word from Flash */
    ldr r3, [r0]

    /* Store the word into RAM */
    str r3, [r1]

    /* Move to next word */
    adds r0, r0, #4
    adds r1, r1, #4

    /* Continue copying */
    b .data_copy_loop


.data_copy_done:


    /* ========================================================
     * 2. CLEAR .bss
     * ========================================================
     *
     * The linker script provides:
     *
     *   _sbss = beginning of .bss
     *   _ebss = end of .bss
     *
     * C requires global/static variables without an explicit
     * initializer to start at zero.
     *
     * Therefore we clear the entire .bss region.
     *
     * Registers:
     *
     *   r1 = current address
     *   r2 = end address
     *   r3 = zero
     * ======================================================== */

    ldr r1, =_sbss
    ldr r2, =_ebss

    /* r3 = 0 */
    movs r3, #0


.bss_clear_loop:

    /* Have we reached the end of .bss? */
    cmp r1, r2

    /* Yes -> continue to main() */
    bhs .bss_clear_done

    /* Store zero */
    str r3, [r1]

    /* Move to next 32-bit word */
    adds r1, r1, #4

    /* Continue clearing */
    b .bss_clear_loop


.bss_clear_done:


    /* ========================================================
     * 3. CALL main()
     * ========================================================
     *
     * BL stores the return address in LR and jumps to main().
     * ======================================================== */

    bl main


    /* ========================================================
     * 4. main() SHOULD NOT RETURN
     * ========================================================
     *
     * If main() returns, stay here forever.
     * ======================================================== */

.main_return:

    b .main_return


.size Reset_Handler, .-Reset_Handler



/* ============================================================
 * DEFAULT EXCEPTION HANDLER
 * ============================================================
 *
 * For now, all exceptions use this handler.
 *
 * Later we can implement individual handlers for:
 *
 *   HardFault
 *   BusFault
 *   UsageFault
 *   SysTick
 *   etc.
 * ============================================================ */

.section .text.Default_Handler, "ax", %progbits

.global Default_Handler
.type Default_Handler, %function

Default_Handler:

    /* Stay here forever */
    b Default_Handler

.size Default_Handler, .-Default_Handler



/* ============================================================
 * EXCEPTION HANDLER SYMBOLS
 * ============================================================
 *
 * All of these handlers currently point to Default_Handler.
 *
 * .set creates aliases:
 *
 *   HardFault_Handler -> Default_Handler
 *
 * etc.
 * ============================================================ */

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