# Decision Log

## Decided

| ID | Decision | Source |
|---|---|---|
| D-001 | Component names are fixed (renamed by D-025): EclipsOS, Eclips Kernel, Monlod, UDFS, ESH, EE, EPM | Project document |
| D-002 | Eclips Kernel is only the name of the kernel | Project owner |
| D-003 | The package manager `EPM` is deferred to a later phase | Project owner |
| D-004 | `disk:N` identifies a physical disk (HD, SSD, floppy or any other); `disk:0`, `disk:1`, `disk:2`, ... are different disks | Project owner |
| D-005 | The Monlod prompt is `Boot>` and nothing else | Project owner |
| D-006 | Classic text-mode visual identity | Project document |
| D-007 | EclipsOS is 100% independent of any other system | Project owner |
| D-008 | The root directory layout is frozen; additions need discussion | Project document |
| D-009 | No drive letters. Resources are identified by namespaces | Project document |
| D-010 | Target CPU architecture is x86_64 | Project owner |
| D-011 | Firmware: BIOS first. UEFI is considered only after BIOS works | Project owner |
| D-012 | Component names were written in uppercase (UDBL, UDFS, UDSH, UDE, UDK). Superseded by D-025 | Project owner |
| D-013 | Programming language is pure C | Project owner |
| D-014 | Monlod performs the switch to long mode. Eclips Kernel is entered already in 64-bit mode | Project owner |
| D-015 | In boot progress lines, dots appear one per second on the same line, and `OK` is printed only after all the dots have appeared | Project owner |
| D-016 | The uptime display (`UP hh:mm:ss`) in the top right corner of the screen is never removed | Project owner |
| D-017 | ESH is written only after processes exist. There is no temporary shell inside the kernel | Project owner |
| D-018 | EclipsOS is highly customizable: a person can replace each component (boot loader, shell and others). Replacing the boot loader is at the user's own risk | Project owner |
| D-019 | The uptime display is part of ESH. Until ESH exists, Eclips Kernel draws it (D-016) | Project owner |
| D-020 | The shell prompt does not contain the user name: it is `disk:0//>`, not `root@disk:0//>`. The current user (for example ROOT) is shown in the top right corner of the screen, where the uptime is | Project owner |
| D-021 | The kernel runs in the higher half of the address space. The lower half belongs to user programs | Project owner |
| D-022 | Eclips Kernel is a microkernel, so that drivers and other services can be replaced without recompiling or replacing the kernel | Project owner |
| D-023 | The build system is a Makefile written for BSD make (bmake), using only features that GNU make also understands | Project owner |
| D-024 | System calls use the `syscall` instruction | Project owner |
| D-025 | The project is renamed. UDOS becomes **EclipsOS**, UDK becomes **Eclips Kernel**, UDBL becomes **Monlod** (Moon + Loader), UDSH becomes **ESH** (Eclips Shell), UDE becomes **EE** (Eclips Executable) and UPM becomes **EPM** (Eclips Package Manager). UDFS keeps its name (Unified Disk File System). File names follow the new names. The `disk:0//` namespace pattern does not change | Project owner |

## Pending

| # | Area | Note |
|---|---|---|
| 2 | Assembly policy | Real mode code cannot be C. See Proposed |
| 3 | Memory management | |
| 4 | Processes and multitasking | |
| 5 | Driver architecture | User-space servers (D-022). The IPC protocol and hardware access are OPEN. |
| 6 | System calls | |
| 7 | ABI | |
| 8 | UDFS specification | |
| 9 | EE binary layout | |
| 10 | ESH syntax | |
| 11 | Namespace internals (registration, access, mounting, path resolution) | |
| 12 | Permission and security model | |
| 13 | System installation | |
| 14 | Partitioning | |
| 15 | Disk image format | |
| 16 | Build tools | |
| 17 | Emulation and testing | |
| 18 | Versioning and distribution | |
| 19 | UEFI support | Deferred by D-011 |
| 20 | IPC model | See specs/microkernel.md |
| 21 | Hardware access for user-space drivers | See specs/microkernel.md |
| 22 | Boot modules and the format of `boot.cfg` | See specs/microkernel.md |

## Proposed

| ID | Proposal |
|---|---|
| P-001 | Assembly is used only where the hardware requires it (boot sectors, real mode code, CPU entry stubs). Everything else is C. |
| P-002 | Assembly sources are GNU as, Intel syntax, built through `gcc` and `ld` from binutils. No extra tool such as NASM is required. |
| P-003 | The disk image is padded to 1440 KiB so it boots both as a floppy and as a hard disk in QEMU. |
| P-004 | Eclips Kernel is loaded at physical address `0x100000` and entered in 64-bit mode with `RDI` pointing to a boot information structure (see `specs/monlod.md`). |
| P-005 | The disk starts with a 16-sector boot area (Stage 1, Stage 2) followed by one UDFS volume at LBA 16. One volume per disk, which matches D-004. |
| P-008 | UDFS version 1: 512-byte blocks, 32-bit block numbers, little-endian, CRC-32 checked superblock. |
| P-009 | UDFS stores files with 64-byte inodes holding up to 4 extents, and tracks free space with a bitmap. |
| P-010 | UDFS directories are arrays of 32-byte entries (inode and a 28-byte name) with `.` and `..`. Names are case sensitive and have at most 28 bytes. |
| P-011 | Monlod finds the kernel by the fixed path `sys/boot/kernel`. `boot.cfg` is not used yet. |
| P-012 | `sys/boot/monlod` holds a copy of the 16-sector boot area. |
| P-013 | `mkudfs` and `udfsck` are host tools written in C and built by the Makefile (`make check` verifies the image). |
| P-014 | Eclips Kernel loads its own GDT (null, 64-bit code, data) and IDT instead of using the ones left by Monlod. |
| P-015 | The 8259 PIC is remapped to vectors 32 to 47. The PIT runs at 100 Hz on IRQ 0. |
| P-016 | Interrupt handlers are written in C with the `interrupt` attribute, without assembly stubs. |
| P-017 | The keyboard is PS/2 with scancode set 1 and a US layout as a stand-in. |
| P-018 | The customization rules R1 to R5 in [customization.md](customization.md): documented and versioned contracts, no dependence on internals, no special treatment of default components. |
| P-019 | The physical memory manager uses 4 KiB frames and a bitmap. It manages only memory from 1 MiB up to 1 GiB, which is the part covered by the direct map. |
| P-020 | The kernel heap is 1 MiB, taken from the physical memory manager, with a first-fit allocator and merging of free blocks. |
| P-021 | Kernel code converts between physical and virtual addresses only through `phys_to_virt` and `virt_to_phys`, so that a different memory layout (D3 in the roadmap) changes one place. |
| P-022 | The virtual memory manager uses 4-level page tables with 4 KiB pages and 2 MiB large pages. It reaches memory only through operations supplied by the caller, so it can be tested on the host. |
| P-023 | Eclips Kernel runs on its own page tables, writable, without separation of code and data. The tables left by Monlod are abandoned. Superseded in part by D-021 and P-025 to P-027: the identity map no longer exists once the kernel runs. |
| P-024 | `vmm_unmap` does not free empty page tables. They are freed when the address space is destroyed. |
| P-025 | The kernel is loaded at physical `0x100000` and linked in the higher half at `0xFFFFFFFF80000000` plus its physical address. Physical memory is also mapped at `0xFFFF800000000000` (direct map). Both cover the first 1 GiB. See [specs/memory-layout.md](specs/memory-layout.md). |
| P-026 | The kernel starts with a small stub linked at its physical address. It builds temporary page tables inside the kernel image, switches to them and jumps to the higher half. Monlod does not change. |
| P-027 | Once running in the higher half the kernel builds its final page tables with no identity map, so addresses in the lower half fault. |
| P-028 | The kernel has its own 16 KiB stack inside its image. The 20 KiB of temporary boot page tables are not reclaimed. |
| P-029 | System call convention: number in `RAX`, arguments in `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9`, result in `RAX` (negative is an error), `RCX` and `R11` destroyed, every other register preserved. See [specs/abi.md](specs/abi.md). |
| P-030 | GDT layout: kernel code `0x08`, kernel data `0x10`, an unused 32-bit user code entry at `0x18`, user data `0x23`, user code `0x2B`, TSS at `0x30`. The order follows from `sysretq`. |
| P-031 | The kernel has a 16 KiB trap stack used by system calls and by interrupts from user mode, and a separate 8 KiB stack for the double fault handler. |
| P-032 | First system calls: `DEBUG_WRITE` (temporary), `EXIT` and `TIME`. |
| P-033 | The kernel never dereferences a user pointer. It translates each page, requires it to be user accessible and copies through the kernel mapping. |
| P-034 | Until the EE loader exists, the first user program is a flat binary linked at `0x400000` and embedded in the kernel image. |
| P-035 | User stack: 8 KiB with its top at `0x00007FFFFFFFF000`. |
| P-036 | Naming choices that follow from D-025 and were not given by the project owner: on-screen messages start with `KERNEL:` (kernel), `MONLOD:` (loader) and `USER:` (the demo program); code identifiers start with `eclips_`, `ECLIPS_` and `monlod_`, `MONLOD_`; the boot information starts with the bytes `MNLD`; the volume label is `EclipsOS`; the loader copy on disk is `sys/boot/monlod`; the disk image is `build/eclipsos.img`. |
| P-006 | Monlod maps the first 1 GiB with 2 MiB pages at handoff. The kernel builds its own page tables. |
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
| How boot modules are described in the boot information (version 2 of the structure) | OPEN |
| Where the configuration that selects the first user program lives, and its format | OPEN |
| Whether Eclips Kernel offers a status line interface for the shell (needed by D-019) | OPEN |
| Keyboard layout to support (the system currently uses US as a stand-in) | OPEN |
