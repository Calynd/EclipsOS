ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text : {
        *(.text.start)
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }

    ASSERT(SIZEOF(.data) == 0, "a user program cannot have writable data yet")
    ASSERT(SIZEOF(.bss) == 0, "a user program cannot have zero-initialized data yet")
}
