.syntax unified
.cpu cortex-m4
.thumb

.section .isr_vector, "a", %progbits

.word _estack
.word Reset_Handler