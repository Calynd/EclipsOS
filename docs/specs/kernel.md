# Eclips Kernel

Eclips Kernel is the EclipsOS kernel. It is a microkernel (D-022). What that means for the code is in [microkernel.md](microkernel.md).

## Entry

**PROPOSED.** Monlod enters Eclips Kernel at physical `0x100000` in 64-bit mode. See [monlod.md](monlod.md) for the handoff contract.

The entry point is `eclips_entry` in `kernel/entry.c`, linked at its physical address. It builds temporary page tables, switches to them and jumps to `eclips_main(const struct monlod_info *info)` in `kernel/main.c`, which runs in the higher half. See [memory-layout.md](memory-layout.md).

## Build

**PROPOSED.** Pure C, freestanding.

| Item | Value |
|---|---|
| Flags | `-ffreestanding -mno-red-zone -mgeneral-regs-only -fcf-protection=none -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-tree-loop-distribute-patterns` |
| Link address | `kernel/entry.c` at `0x100000`. Everything else at `0xFFFFFFFF80000000` plus its physical address, compiled with `-mcmodel=kernel`. Defined in `kernel/kernel.ld`. |
| Output | Flat binary produced with `objcopy -O binary` |

Consequences:

- `-mgeneral-regs-only` keeps the compiler from using SSE registers, which are not enabled. It is also what allows interrupt handlers written in C.
- `-mno-red-zone` is required because interrupts use the kernel stack.
- The flat binary does not contain `.bss`, the kernel stack or the boot page tables. The kernel zeroes `.bss` at the start of `eclips_main`, using `__bss_start` and `__bss_end` from the linker script. The entry stub zeroes the boot page tables itself.
- The entry stub is compiled without the kernel code model and may not call anything outside its own section.
- The kernel image can have at most 896 sectors (see [monlod.md](monlod.md)).

## Current state (milestone 6)

| File | Role |
|---|---|
| `kernel/entry.c` | Entry stub at the physical address: temporary page tables, switch to the higher half |
| `kernel/boot_tables.h` | Builds the temporary page tables (shared with a host test) |
| `kernel/layout.h` | Address space constants |
| `kernel/tss.c`, `kernel/tss.h` | Task state segment, the trap stack and the double fault stack |
| `kernel/msr.h` | Access to model specific registers |
| `kernel/syscall.c`, `kernel/syscall.h` | The `syscall` entry code, its setup and the dispatcher |
| `kernel/uaccess.c`, `kernel/uaccess.h` | Safe copy from user memory (pure logic) |
| `kernel/user.c`, `kernel/user.h` | Starts and ends the first user program |
| `kernel/user_image.S` | Embeds the user program binary in the kernel image |
| `user/` | The first user program (`hello.c`), its helpers (`eclips.h`) and its linker script |
| `kernel/main.c` | Main function of the higher half, boot information check, initialization order, keyboard echo loop and uptime display |
| `kernel/vga.c`, `kernel/vga.h` | VGA text console at `0xB8000` with scrolling, backspace, hardware cursor and a status line writer |
| `kernel/monlod.h` | Boot information structures shared with Monlod |
| `kernel/gdt.c`, `kernel/gdt.h` | Kernel GDT: null, 64-bit code (`0x08`), data (`0x10`) |
| `kernel/idt.c`, `kernel/idt.h` | IDT with 256 interrupt gates. Unused vectors point to a handler that only returns. |
| `kernel/exception.c`, `kernel/exception.h` | Handlers for vectors 0 to 31 |
| `kernel/pic.c`, `kernel/pic.h` | 8259 PIC remap, mask, unmask and end of interrupt |
| `kernel/timer.c`, `kernel/timer.h` | PIT channel 0 and the tick counter |
| `kernel/keyboard.c`, `kernel/keyboard.h` | PS/2 keyboard interrupt handler and input buffer |
| `kernel/kbd_map.c`, `kernel/kbd_map.h` | Scancode to character translation (pure logic) |
| `kernel/charbuf.c`, `kernel/charbuf.h` | Ring buffer for characters (pure logic) |
| `kernel/pmm.c`, `kernel/pmm.h` | Physical frame allocator (bitmap, pure logic) |
| `kernel/heap.c`, `kernel/heap.h` | Kernel heap allocator (pure logic) |
| `kernel/memory.c`, `kernel/memory.h` | Builds the allocators from the Monlod memory map. `kmalloc`, `kfree`, frame allocation, `phys_to_virt`, self-test. |
| `kernel/vmm.c`, `kernel/vmm.h` | Page table management for one address space: map, map large, unmap, translate, destroy (pure logic) |
| `kernel/vm.c`, `kernel/vm.h` | The kernel address space: builds the kernel page tables, loads them and runs the self-test |
| `kernel/io.h`, `kernel/interrupt.h` | Port I/O, interrupt frame and `sti`/`cli`/`hlt` helpers |

The keyboard, the screen console and the uptime display are in the kernel for now. With the microkernel decision they are to move to user-space servers, and they stay until their replacements work (D-016). See [microkernel.md](microkernel.md).

### Initialization order

0. Entry stub: temporary page tables, kernel stack, jump to the higher half (see [memory-layout.md](memory-layout.md)).
1. Clear `.bss` and check the boot information.
2. Print boot drive and memory summary.
3. Load the kernel GDT and reload the segment registers.
4. Load the IDT and install the exception handlers.
5. Initialize physical memory and the heap, and run the memory self-test.
6. Build the kernel page tables, load them and run the virtual memory self-test.
7. Set up the system call entry, with the task state segment loaded earlier by the GDT.
8. Remap the PIC and mask every IRQ except the cascade.
8. Start the timer (100 Hz) and unmask IRQ 0.
10. Initialize the keyboard and unmask IRQ 1.
11. Enable interrupts.
12. Start the first user program. When it exits the kernel continues with the keyboard echo loop. See [abi.md](abi.md).

### Memory management

**PROPOSED.**

| Item | Behavior |
|---|---|
| Frames | 4 KiB, tracked with a bitmap (1 bit per frame, set means used). |
| Managed range | From 1 MiB up to 1 GiB. Memory below 1 MiB (BIOS data, Monlod structures, the boot stack and the current page tables) is never handed out. Memory above 1 GiB is not reachable until the kernel has its own page tables. |
| Source of free memory | The usable entries of the Monlod memory map, clipped to the managed range. |
| Reserved | The kernel image, from `__kernel_start` to `__kernel_end` (end of `.bss`, page aligned). |
| Allocation | Next-fit for single frames, first-fit for contiguous runs. A failed allocation returns `PMM_NO_FRAME`. Freeing detects unaligned, out of range and double frees. |
| Kernel heap | 1 MiB of contiguous frames. First-fit blocks with 16-byte headers and 16-byte alignment, split on allocation and merged on free. |
| Address conversion | `phys_to_virt` adds the direct map base (`0xFFFF800000000000`). `virt_to_phys` handles direct map and kernel window addresses. |

At boot the self-test allocates and frees frames, writes to them, checks that a double free is rejected and exercises `kmalloc` and `kfree`. It prints `KERNEL: MEMORY SELF-TEST OK`, or `KERNEL: MEMORY SELF-TEST FAILED, CODE n` and halts.

With the default QEMU memory (128 MB) the expected lines are:

```text
KERNEL: PHYSICAL MEMORY 32212 FRAMES FREE (128848 KIB)
KERNEL: KERNEL HEAP 1024 KIB
KERNEL: MEMORY SELF-TEST OK
```

The count comes from the second usable region of the memory map (`0x100000` to `0x7FE0000`, 32480 frames) minus 12 frames of kernel image and 256 frames of heap.

### Virtual memory

**PROPOSED.**

| Item | Behavior |
|---|---|
| Tables | 4 levels (PML4, PDPT, PD, PT). Pages of 4 KiB, and large pages of 2 MiB. |
| Operations | `vmm_map`, `vmm_map_large`, `vmm_unmap`, `vmm_translate`, `vmm_space_init`, `vmm_space_destroy`. Errors: no memory, already mapped, not mapped, invalid argument, inside a large page. |
| Access to memory | The manager reaches table frames and allocates them only through operations given by the caller (`struct vmm_ops`). This is what allows the host tests, and one place to change when the kernel layout changes. |
| Flags | Write, user, write-through, cache disable, no-execute. A user mapping also sets the user bit on every table level of its path. |
| Kernel address space | The direct map and the kernel window of the first 1 GiB, with 2 MiB pages, writable, and nothing else. There is no identity map, so the lower half faults. |
| Flushing | `invlpg` after every map and unmap. |

The self-test maps a frame at `0x200000000` (in the lower half), writes through the new address and reads through the direct map, checks that mapping twice fails, unmaps, checks the translation of the VGA buffer through the direct map and of a kernel function through the kernel window, checks that unmapping inside a large page is refused, and checks that addresses `0` and `0x1000` are not mapped.

The expected lines are below. The numbers depend on the size of the kernel image, which is rounded up to whole frames, so they can be one or two frames away on another compiler. With the default QEMU memory (128 MB) and the reference build, the kernel image is 22 frames:

```text
KERNEL: RUNNING IN THE HIGHER HALF AT 0xFFFFFFFF801000E0
KERNEL: PHYSICAL MEMORY 32202 FRAMES FREE (128808 KIB)
KERNEL: PAGE TABLES LOADED, LOW ADDRESSES UNMAPPED, 32197 FRAMES FREE
KERNEL: VIRTUAL MEMORY SELF-TEST OK
```

The address on the first line starts with `0xFFFFFFFF8010`. The 5 frames of the final tables are a PML4, two PDPTs and two PDs (the direct map and the kernel window).

To see a page fault report, build with one of these tests:

```text
make clean
make KERNEL_DEFINES=-DECLIPS_NULL_TEST run
make clean
make KERNEL_DEFINES=-DECLIPS_PAGEFAULT_TEST run
```

The first reads address 0 and must report `KERNEL: EXCEPTION 14 PAGE FAULT`, `KERNEL: FAULT ADDRESS 0x0` and `KERNEL: PAGE NOT PRESENT, READ, SUPERVISOR`. The second reads the address that the self-test unmapped and must report `KERNEL: FAULT ADDRESS 0x200000000` with the same decoding.

### Interrupts

**PROPOSED.**

| Vector | Source |
|---|---|
| 0 to 31 | CPU exceptions. Each one prints a report and halts the system. |
| 32 | IRQ 0, PIT timer (100 Hz) |
| 33 | IRQ 1, PS/2 keyboard |
| 34 to 47 | Other IRQs. Masked. |
| 48 to 255 | Ignored (the handler only returns) |

Handlers are written in C with the compiler's `interrupt` attribute. There is no assembly stub.

### Exception report

For an exception the kernel prints the vector number and name, the error code (for the vectors that have one), `RIP`, `CS`, `RFLAGS` and `RSP`, and for a page fault the address from `CR2`. Then it prints `KERNEL: SYSTEM HALTED` and stops.

To test it, build with a fault at the end of the initialization:

```text
make clean
make KERNEL_DEFINES=-DECLIPS_FAULT_TEST run
```

The expected result is `KERNEL: EXCEPTION 6 INVALID OPCODE`.

### Keyboard

PS/2 keyboard, scancode set 1 (the translation the BIOS leaves enabled in the controller), US layout. Handled keys: letters, digits, the punctuation keys of the main block, Shift (left and right), Caps Lock, Enter, Backspace, Tab, Escape and Space. Extended keys (prefix `0xE0`) are ignored.

The US layout is a stand-in. The layout to support is **OPEN** (see [../decisions.md](../decisions.md)).

### Screen behavior

After the initialization lines the kernel shows the prompt `> ` and echoes what is typed (up to 70 characters per line, Backspace edits the line, Enter starts a new line). The top right corner shows the uptime as `UP hh:mm:ss`, updated once per second from the timer. This is only a test of the timer and the keyboard until ESH exists.

### Tests

`make test` builds and runs host tests for the logic that does not depend on the hardware: `tests/test_kbd_map.c` (translation, Shift, Caps Lock, release and extended codes), `tests/test_charbuf.c` (order, capacity, wraparound) and `tests/test_idt.c` (byte layout of an IDT entry), `tests/test_pmm.c` (frame allocator: ranges, rounding, exhaustion, double free, contiguous runs) and `tests/test_heap.c` (alignment, splitting, merging, double free, exhaustion and a randomized stress run) and `tests/test_descriptors.c` and `tests/test_uaccess.c` (see [abi.md](abi.md)), `tests/test_boot_tables.c` (the temporary page tables of the entry stub: identity map, direct map and kernel window, and what is not mapped) and `tests/test_vmm.c` (mapping, translation, tables created per path, user bit, large pages, errors, out of memory and destroying a space, using simulated physical memory).

## Known limits

- Only the double fault handler has its own stack. A stack overflow elsewhere is reported as a double fault.
- Only the 8259 PIC is used. APIC and multiple processors are not supported.
- The keyboard code does not talk to the controller (no LEDs, no layout selection, no error handling).
- Exceptions are fatal. Nothing is recovered yet.
- The physical memory manager sees only the first 1 GiB, and the heap does not grow.
- `vmm_unmap` does not free page tables that become empty. They are freed when the whole space is destroyed. The self-test leaves 3 table frames allocated.
- The kernel mappings are writable everywhere, including its own code. There is no separation between code and data yet.
- The no-execute flag exists in the interface but must not be used yet: the CPU rejects a no-execute entry as a reserved bit fault until `EFER.NXE` is enabled.
- Status: milestone 3 (GDT, IDT, PIC, timer with the uptime display, keyboard echo) ran correctly in QEMU. Milestone 4 (memory) ran correctly in QEMU with the expected values (32212 frames free, 1024 KiB heap, self-test OK). Milestone 5 (virtual memory) ran correctly in QEMU. Milestone 5b (higher half) ran correctly in QEMU. Milestone 6 (system calls and user mode) passes the host tests and was checked by disassembly of the entry code, the setup of the model specific registers and the return to user mode, but has not been executed in QEMU yet. The exception report has not been executed yet (`-DECLIPS_FAULT_TEST`). The code was also checked by disassembly (no SSE instructions, handler stack frames, GDT reload sequence) and by the host tests.

## Open items

| Item | Status |
|---|---|
| Layout of user address spaces inside the lower half | OPEN |
| Processes, scheduling and system calls | OPEN |
| Keyboard layout and a key repeat policy | OPEN |
| Interface of drivers: IPC protocol and hardware access | OPEN |
