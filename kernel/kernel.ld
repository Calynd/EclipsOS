ENTRY(udk_entry)

KERNEL_PHYS = 0x100000;
KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;

SECTIONS
{
    . = KERNEL_PHYS;
    __kernel_phys_start = .;

    .boot : {
        *(.boot.entry)
        *(.boot .boot.*)
    }

    . = ALIGN(16) + KERNEL_VIRT_BASE;

    .text : AT(ADDR(.text) - KERNEL_VIRT_BASE) {
        *(.text .text.*)
    }

    . = ALIGN(16);
    .rodata : AT(ADDR(.rodata) - KERNEL_VIRT_BASE) {
        *(.rodata .rodata.*)
    }

    . = ALIGN(16);
    .data : AT(ADDR(.data) - KERNEL_VIRT_BASE) {
        *(.data .data.*)
    }

    . = ALIGN(16);
    .bss : AT(ADDR(.bss) - KERNEL_VIRT_BASE) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    .stack (NOLOAD) : {
        __stack_bottom = .;
        . += 0x4000;
        __stack_top = .;
    }

    . = ALIGN(4096);
    .boot_tables (NOLOAD) : {
        __boot_tables_virt = .;
        . += 0x5000;
    }

    . = ALIGN(4096);
    __kernel_virt_end = .;
    __kernel_phys_end = __kernel_virt_end - KERNEL_VIRT_BASE;
    __boot_tables_phys = __boot_tables_virt - KERNEL_VIRT_BASE;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
