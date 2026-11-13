ENTRY(_start)
MEMORY
{
    rom (rx) : ORIGIN = 0x08000000, LENGTH = 32M
    ewram (rw) : ORIGIN = 0x02000000, LENGTH = 256K
}
SECTIONS
{
    .text : {
        KEEP(*(.gba_header))
        KEEP(*(.text.reset))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > rom
    .data : {
        . = ALIGN(4);
        __data_start = .;
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > ewram AT> rom
    __data_load = LOADADDR(.data);
    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*) *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > ewram
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) }
    ASSERT(SIZEOF(.text) + SIZEOF(.data) <= 32M, "GBA ROM budget exceeded")
    ASSERT(__bss_end <= ORIGIN(ewram) + LENGTH(ewram), "GBA work RAM budget exceeded")
}
