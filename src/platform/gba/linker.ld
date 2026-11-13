ENTRY(_start)
MEMORY
{
    rom (rx) : ORIGIN = 0x08000000, LENGTH = 32M
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 28K
    ewram (rw) : ORIGIN = 0x02000000, LENGTH = 256K
}
SECTIONS
{
    .text : {
        KEEP(*(.gba_header))
        KEEP(*(.text.reset))
        *(EXCLUDE_FILE (*world.o *enemies.o *deceptus.o *mechanisms.o *main.o) .text*)
        *(.rodata*)
        . = ALIGN(4);
    } > rom
    .iwram : {
        . = ALIGN(4);
        __iwram_start = .;
        *(.iwram*)
        *world.o(.text*)
        *enemies.o(.text*)
        *deceptus.o(.text*)
        *mechanisms.o(.text*)
        *main.o(.text*)
        . = ALIGN(4);
        __iwram_end = .;
    } > iwram AT> rom
    __iwram_load = LOADADDR(.iwram);
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
    ASSERT(SIZEOF(.text) + SIZEOF(.iwram) + SIZEOF(.data) <= 32M, "GBA ROM budget exceeded")
    ASSERT(__bss_end <= ORIGIN(ewram) + LENGTH(ewram), "GBA work RAM budget exceeded")
}
