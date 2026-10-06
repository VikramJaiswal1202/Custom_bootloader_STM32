MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08008000, LENGTH = 480K  /* sector 2-7 of the memory*/
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K  /* 128KB SRAM 1  */
}

_estack = ORIGIN(RAM) + LENGTH(RAM);  /* Stack starts from where the ram ends*/

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(128);
        KEEP(*(.isr_vector))
        . = ALIGN(128);
    } > FLASH

    .text :
    {
        *(.text)
        *(.text.*)
    } > FLASH

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        _sdata = .;
        *(.data)
        *(.data.*)
        _edata = .;
    } > RAM AT > FLASH

    .bss :
    {
        _sbss = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _ebss = .;
    } > RAM

    . = ALIGN(8);
    _end = .;
}