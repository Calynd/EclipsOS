# Memory Layout

**PROPOSED**, except where marked. D-021 (kernel in the higher half) is **DECIDED**.

## Virtual address space

| Range | Use |
|---|---|
| `0x0000000000000000` to `0x00007FFFFFFFFFFF` | Lower half. Reserved for user programs. Nothing is mapped in it today, so a null pointer access is a page fault. |
| `0xFFFF800000000000` to `0xFFFF80003FFFFFFF` | Direct map: physical memory from 0 to 1 GiB, writable, 2 MiB pages. `phys_to_virt(p)` is `p + 0xFFFF800000000000`. |
| `0xFFFFFFFF80000000` to `0xFFFFFFFFBFFFFFFF` | Kernel window: physical memory from 0 to 1 GiB. The kernel image runs here. Virtual address `0xFFFFFFFF80000000 + p` is physical `p`. |

The constants are in `kernel/layout.h`. The kernel window base is repeated in `kernel/kernel.ld`, and the Makefile checks that `udk_entry` ends up at `0x100000` and `udk_main` in the higher half.

The direct map and the kernel window are limited to the first 1 GiB, the same limit as the physical memory manager.

## Physical memory

| Address | Use |
|---|---|
| `0x00000` to `0x004FF` | BIOS data. Never handed out. |
| `0x01000` to `0x01610` | Boot information from UDBL (memory map). Read by the kernel through the direct map. |
| `0x02000` to `0x04FFF` | Page tables of UDBL, abandoned once the kernel runs. |
| `0x07C00` to `0x08E00` | UDBL stages, no longer used after the handoff. |
| `0x80000` to `0x8FFFF` | Boot stack of UDBL. The kernel leaves it at the first instruction of its entry stub. |
| `0x100000` | Kernel load address and entry point (`udk_entry`) |
| `0x100000` to `__kernel_phys_end` | Kernel image: low entry stub, text, read-only data, data, `.bss`, kernel stack (16 KiB) and the temporary boot page tables (20 KiB). With the current build it ends at `0x116000`. Reserved in the physical memory manager. |
| `__kernel_phys_end` to 1 GiB | Managed by the physical memory manager |

Memory below 1 MiB is not handed out by the physical memory manager.

## Entering the higher half

UDBL jumps to physical `0x100000` in 64-bit mode (see [udbl.md](udbl.md)). The kernel is linked so that its first section, `.boot`, runs at its physical address, and every other section runs at its higher half address while being loaded right after it. The flat binary is therefore a plain copy of the image, and UDBL does not know about the higher half.

The entry stub (`udk_entry` in `kernel/entry.c`) does the following, using no memory except its own image and no function outside the `.boot` section:

1. Builds five temporary page tables inside the kernel image (`.boot_tables`): a PML4, three PDPTs and one PD. The PD maps 0 to 1 GiB with 2 MiB pages, and the three PDPTs share it, so the same physical memory appears at the identity address, in the direct map and in the kernel window. The code that builds them is `kernel/boot_tables.h`, which has a host test.
2. Loads them into `CR3`. The stub keeps running at its low address, which is why the identity map is included.
3. Switches to the kernel stack (in the higher half) and jumps to `udk_main`, passing the boot information pointer converted to its direct map address.

`udk_main` then clears `.bss`, loads the kernel GDT and IDT, initializes physical memory, and builds the final page tables with the virtual memory manager: only the direct map and the kernel window, with no identity map. Loading them is the moment the low addresses stop working. The temporary tables, 20 KiB, are not reclaimed.

## What the kernel needs from a boot loader

This is the part of the handoff that matters to a replacement loader (see [customization.md](../customization.md)):

- The kernel is entered at physical `0x100000`, in 64-bit mode, with interrupts disabled.
- Physical memory from 0 up to the end of the kernel image (`__kernel_phys_end`, and the boot information) must be identity mapped. UDBL maps the first 1 GiB, which is the recommended choice.
- `RDI` holds the physical address of the boot information structure.
- The kernel does not depend on the page tables, the GDT or the stack of the loader beyond the first instructions of the entry stub.

## Limits

- Only the first 1 GiB of physical memory is mapped.
- The direct map and the kernel window are writable everywhere and not marked no-execute. Separating code from data is future work.
- The kernel image must stay in the first 1 GiB and below the identity map the loader provides.
