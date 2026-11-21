# Decision Log

## Decided

| ID | Decision | Source |
|---|---|---|
| D-001 | Component names UDBL, UDFS, UDSH, UDE, UDK are fixed | Project document |
| D-002 | UDK is only the name of the kernel | Project owner |
| D-003 | The package manager `udp` is deferred to a later phase | Project owner |
| D-004 | `disk:N` identifies a physical disk (HD, SSD, floppy or any other); `disk:0`, `disk:1`, `disk:2`, ... are different disks | Project owner |
| D-005 | The UDBL prompt is `Boot>` and nothing else | Project owner |
| D-006 | Classic visual identity, in the style of MS-DOS | Project document |
| D-007 | UDOS is 100% independent of any other system; KOREX may only inspire ideas for the file system | Project owner |
| D-008 | The root directory layout is frozen; additions need discussion | Project document |
| D-009 | No drive letters such as `C:\` or `A:\` | Project document |
| D-010 | Target CPU architecture is x86_64 | Project owner |
| D-011 | Firmware: BIOS first. UEFI is considered only after BIOS works | Project owner |
| D-012 | UDOS component names are always written in uppercase: UDBL, UDFS, UDSH, UDE, UDK | Project owner |
| D-013 | Programming language is pure C | Project owner |
| D-014 | UDBL performs the switch to long mode. UDK is entered already in 64-bit mode | Project owner |
| D-015 | In boot progress lines, dots appear one per second on the same line, and `OK` is printed only after all the dots have appeared | Project owner |
| D-016 | The uptime display (`UP hh:mm:ss`) in the top right corner of the screen is never removed | Project owner |
| D-017 | UDSH is written only after processes exist. There is no temporary shell inside the kernel | Project owner |
| D-018 | UDOS is highly customizable: a person can replace each component (boot loader, shell and others). Replacing the boot loader is at the user's own risk | Project owner |
| D-019 | The uptime display is part of UDSH. Until UDSH exists, UDK draws it (D-016) | Project owner |
| D-020 | The shell prompt does not contain the user name: it is `disk:0//>`, not `root@disk:0//>`. The current user (for example ROOT) is shown in the top right corner of the screen, where the uptime is | Project owner |

## Pending

| # | Area | Note |
|---|---|---|
| 1 | Kernel architecture | Still to be decided |
| 2 | Assembly policy | Real mode code cannot be C. See Proposed |
| 3 | Memory management | |
| 4 | Processes and multitasking | |
| 5 | Driver architecture | |
| 6 | System calls | |
| 7 | ABI | |
| 8 | UDFS specification | |
| 9 | UDE binary layout | |
| 10 | UDSH syntax | |
| 11 | Namespace internals (registration, access, mounting, path resolution) | |
| 12 | Permission and security model | |
| 13 | System installation | |
| 14 | Partitioning | |
| 15 | Disk image format | |
| 16 | Build tools | |
| 17 | Emulation and testing | |
| 18 | Versioning and distribution | |
| 19 | UEFI support | Deferred by D-011 |

## Proposed

| ID | Proposal |
|---|---|
| P-001 | Assembly is used only where the hardware requires it (boot sectors, real mode code, CPU entry stubs). Everything else is C. |
| P-002 | Assembly sources are GNU as, Intel syntax, built through `gcc` and `ld` from binutils. No extra tool such as NASM is required. |
| P-003 | The disk image is padded to 1440 KiB so it boots both as a floppy and as a hard disk in QEMU. |
| P-004 | UDK is loaded at physical address `0x100000` and entered in 64-bit mode with `RDI` pointing to a boot information structure (see `specs/udbl.md`). |
| P-005 | The disk starts with a 16-sector boot area (Stage 1, Stage 2) followed by one UDFS volume at LBA 16. One volume per disk, which matches D-004. |
| P-008 | UDFS version 1: 512-byte blocks, 32-bit block numbers, little-endian, CRC-32 checked superblock. |
| P-009 | UDFS stores files with 64-byte inodes holding up to 4 extents, and tracks free space with a bitmap. |
| P-010 | UDFS directories are arrays of 32-byte entries (inode and a 28-byte name) with `.` and `..`. Names are case sensitive and have at most 28 bytes. |
| P-011 | UDBL finds the kernel by the fixed path `sys/boot/kernel`. `boot.cfg` is not used yet. |
| P-012 | `sys/boot/udbl` holds a copy of the 16-sector boot area. |
| P-013 | `mkudfs` and `udfsck` are host tools written in C and built by the Makefile (`make check` verifies the image). |
| P-014 | UDK loads its own GDT (null, 64-bit code, data) and IDT instead of using the ones left by UDBL. |
| P-015 | The 8259 PIC is remapped to vectors 32 to 47. The PIT runs at 100 Hz on IRQ 0. |
| P-016 | Interrupt handlers are written in C with the `interrupt` attribute, without assembly stubs. |
| P-017 | The keyboard is PS/2 with scancode set 1 and a US layout as a stand-in. |
| P-018 | The customization rules R1 to R5 in [customization.md](customization.md): documented and versioned contracts, no dependence on internals, no special treatment of default components. |
| P-019 | The physical memory manager uses 4 KiB frames and a bitmap. It manages only memory from 1 MiB up to 1 GiB, which is the part the kernel can reach with the identity mapping left by UDBL. |
| P-020 | The kernel heap is 1 MiB, taken from the physical memory manager, with a first-fit allocator and merging of free blocks. |
| P-021 | Kernel code converts between physical and virtual addresses only through `phys_to_virt` and `virt_to_phys`, so that a different memory layout (D3 in the roadmap) changes one place. |
| P-022 | The virtual memory manager uses 4-level page tables with 4 KiB pages and 2 MiB large pages. It reaches memory only through operations supplied by the caller, so it can be tested on the host. |
| P-023 | UDK runs on its own page tables: an identity map of the first 1 GiB with 2 MiB pages, writable, without separation of code and data. The tables left by UDBL are abandoned. |
| P-024 | `vmm_unmap` does not free empty page tables. They are freed when the address space is destroyed. |
| P-006 | UDBL maps the first 1 GiB with 2 MiB pages at handoff. The kernel builds its own page tables. |
| P-007 | The number of dots per boot line is fixed per build (`BOOT_DOTS` in the Makefile, default 18, as in the visual target). With one dot per second the kernel line takes about 18 seconds. |

## Open questions

| Question | Status |
|---|---|
| How are partitions of one disk addressed, or does one disk hold exactly one UDFS volume? | OPEN |
| Is the system disk always `disk:0`, regardless of enumeration order? | OPEN |
| Do `dev/` and the non-disk namespaces (`ram:`, `net:`) overlap in purpose? | OPEN |
| Case sensitivity of UDFS names versus a classic case-insensitive look | OPEN |
| Permission model, timestamps, links, journaling and recovery for UDFS | OPEN |
| Does the top right corner show the user and the uptime together, or does the user replace the uptime? D-016 says the uptime is never removed, and D-020 puts the user in the same corner | OPEN |
| Where the configuration that selects the first user program lives, and its format | OPEN |
| Whether UDK offers a status line interface for the shell (needed by D-019) | OPEN |
| Keyboard layout to support (the system currently uses US as a stand-in) | OPEN |
| Case of the `udbl` file name in `sys/boot/` given D-012 | OPEN |
| Case of the package manager name given D-012 (`udp` would collide with the UDP protocol if written `UDP`) | OPEN |
