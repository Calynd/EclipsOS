# UDK

UDK is the UDOS kernel. Its architecture (monolithic, microkernel or other) is **OPEN**.

## Entry

**PROPOSED.** UDBL enters UDK at `0x100000` in 64-bit mode. See [udbl.md](udbl.md) for the handoff contract.

The entry function is `udk_main(const struct udbl_info *info)` in `kernel/main.c`. The linker script places it first in the binary so that the load address is the entry point.

## Build

**PROPOSED.** Pure C, freestanding.

| Item | Value |
|---|---|
| Flags | `-ffreestanding -mno-red-zone -mgeneral-regs-only -fcf-protection=none -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-tree-loop-distribute-patterns` |
| Link address | `0x100000`, defined in `kernel/kernel.ld` |
| Output | Flat binary produced with `objcopy -O binary` |

Consequences:

- `-mgeneral-regs-only` keeps the compiler from using SSE registers, which are not enabled. It is also what allows interrupt handlers written in C.
- `-mno-red-zone` is required because interrupts use the kernel stack.
- The flat binary does not contain `.bss`. The kernel zeroes it at the start of `udk_main`, using `__bss_start` and `__bss_end` from the linker script.
- The kernel image can have at most 896 sectors (see [udbl.md](udbl.md)).

## Current state (milestone 5)

| File | Role |
|---|---|
| `kernel/main.c` | Entry, boot information check, initialization order, keyboard echo loop and uptime display |
| `kernel/vga.c`, `kernel/vga.h` | VGA text console at `0xB8000` with scrolling, backspace, hardware cursor and a status line writer |
| `kernel/udbl.h` | Boot information structures shared with UDBL |
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
| `kernel/memory.c`, `kernel/memory.h` | Builds the allocators from the UDBL memory map. `kmalloc`, `kfree`, frame allocation, `phys_to_virt`, self-test. |
| `kernel/vmm.c`, `kernel/vmm.h` | Page table management for one address space: map, map large, unmap, translate, destroy (pure logic) |
| `kernel/vm.c`, `kernel/vm.h` | The kernel address space: builds the kernel page tables, loads them and runs the self-test |
| `kernel/io.h`, `kernel/interrupt.h` | Port I/O, interrupt frame and `sti`/`cli`/`hlt` helpers |

### Initialization order

1. Clear `.bss` and check the boot information.
2. Print boot drive and memory summary.
3. Load the kernel GDT and reload the segment registers.
4. Load the IDT and install the exception handlers.
5. Initialize physical memory and the heap, and run the memory self-test.
6. Build the kernel page tables, load them and run the virtual memory self-test.
7. Remap the PIC and mask every IRQ except the cascade.
8. Start the timer (100 Hz) and unmask IRQ 0.
9. Initialize the keyboard and unmask IRQ 1.
10. Enable interrupts.

### Memory management

**PROPOSED.**

| Item | Behavior |
|---|---|
| Frames | 4 KiB, tracked with a bitmap (1 bit per frame, set means used). |
| Managed range | From 1 MiB up to 1 GiB. Memory below 1 MiB (BIOS data, UDBL structures, the boot stack and the current page tables) is never handed out. Memory above 1 GiB is not reachable until the kernel has its own page tables. |
| Source of free memory | The usable entries of the UDBL memory map, clipped to the managed range. |
| Reserved | The kernel image, from `__kernel_start` to `__kernel_end` (end of `.bss`, page aligned). |
| Allocation | Next-fit for single frames, first-fit for contiguous runs. A failed allocation returns `PMM_NO_FRAME`. Freeing detects unaligned, out of range and double frees. |
| Kernel heap | 1 MiB of contiguous frames. First-fit blocks with 16-byte headers and 16-byte alignment, split on allocation and merged on free. |
| Address conversion | `phys_to_virt` and `virt_to_phys` (identity today). |

At boot the self-test allocates and frees frames, writes to them, checks that a double free is rejected and exercises `kmalloc` and `kfree`. It prints `UDK: MEMORY SELF-TEST OK`, or `UDK: MEMORY SELF-TEST FAILED, CODE n` and halts.

With the default QEMU memory (128 MB) the expected lines are:

```text
UDK: PHYSICAL MEMORY 32212 FRAMES FREE (128848 KIB)
UDK: KERNEL HEAP 1024 KIB
UDK: MEMORY SELF-TEST OK
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
| Kernel address space | Identity map of the first 1 GiB with 2 MiB pages, writable. The tables left by UDBL (below 1 MiB) are abandoned. |
| Flushing | `invlpg` after every map and unmap. |

The self-test maps a frame at `0x200000000` (outside the identity map), writes through the new address and reads through the physical one, checks that mapping twice fails, unmaps, checks the translation of the VGA buffer through a large page and checks that unmapping inside a large page is refused.

With the default QEMU memory the new expected lines are:

```text
UDK: PAGE TABLES LOADED, IDENTITY MAP 1 GIB, 32209 FRAMES FREE
UDK: VIRTUAL MEMORY SELF-TEST OK
```

The 3 frames are the PML4, the PDPT and the PD of the identity map.

To see a page fault report, build with a read from the address that the self-test unmapped:

```text
make clean
make KERNEL_DEFINES=-DUDK_PAGEFAULT_TEST run
```

The expected result is `UDK: EXCEPTION 14 PAGE FAULT`, `UDK: ERROR CODE 0x0`, `UDK: FAULT ADDRESS 0x200000000` and `UDK: PAGE NOT PRESENT, READ, SUPERVISOR`.

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

For an exception the kernel prints the vector number and name, the error code (for the vectors that have one), `RIP`, `CS`, `RFLAGS` and `RSP`, and for a page fault the address from `CR2`. Then it prints `UDK: SYSTEM HALTED` and stops.

To test it, build with a fault at the end of the initialization:

```text
make clean
make KERNEL_DEFINES=-DUDK_FAULT_TEST run
```

The expected result is `UDK: EXCEPTION 6 INVALID OPCODE`.

### Keyboard

PS/2 keyboard, scancode set 1 (the translation the BIOS leaves enabled in the controller), US layout. Handled keys: letters, digits, the punctuation keys of the main block, Shift (left and right), Caps Lock, Enter, Backspace, Tab, Escape and Space. Extended keys (prefix `0xE0`) are ignored.

The US layout is a stand-in. The layout to support is **OPEN** (see [../decisions.md](../decisions.md)).

### Screen behavior

After the initialization lines the kernel shows the prompt `> ` and echoes what is typed (up to 70 characters per line, Backspace edits the line, Enter starts a new line). The top right corner shows the uptime as `UP hh:mm:ss`, updated once per second from the timer. This is only a test of the timer and the keyboard until UDSH exists.

### Tests

`make test` builds and runs host tests for the logic that does not depend on the hardware: `tests/test_kbd_map.c` (translation, Shift, Caps Lock, release and extended codes), `tests/test_charbuf.c` (order, capacity, wraparound) and `tests/test_idt.c` (byte layout of an IDT entry), `tests/test_pmm.c` (frame allocator: ranges, rounding, exhaustion, double free, contiguous runs) and `tests/test_heap.c` (alignment, splitting, merging, double free, exhaustion and a randomized stress run) and `tests/test_vmm.c` (mapping, translation, tables created per path, user bit, large pages, errors, out of memory and destroying a space, using simulated physical memory).

## Known limits

- No TSS and no interrupt stack table. A stack overflow in a handler ends in a triple fault.
- Only the 8259 PIC is used. APIC and multiple processors are not supported.
- The keyboard code does not talk to the controller (no LEDs, no layout selection, no error handling).
- Exceptions are fatal. Nothing is recovered yet.
- The physical memory manager sees only the first 1 GiB, and the heap does not grow.
- `vmm_unmap` does not free page tables that become empty. They are freed when the whole space is destroyed. The self-test leaves 2 table frames allocated.
- The kernel is identity mapped and writable everywhere, including its own code. There is no separation between code and data yet, and the low page (address 0) is mapped.
- The no-execute flag exists in the interface but must not be used yet: the CPU rejects a no-execute entry as a reserved bit fault until `EFER.NXE` is enabled.
- The kernel is not in the higher half. That depends on D3 in the roadmap.
- Status: milestone 3 (GDT, IDT, PIC, timer with the uptime display, keyboard echo) ran correctly in QEMU. Milestone 4 (memory) ran correctly in QEMU with the expected values (32212 frames free, 1024 KiB heap, self-test OK). Milestone 5 (virtual memory) passes the host tests and was checked by disassembly, but has not been executed in QEMU yet. The exception report has not been executed yet (`-DUDK_FAULT_TEST`). The code was also checked by disassembly (no SSE instructions, handler stack frames, GDT reload sequence) and by the host tests.

## Open items

| Item | Status |
|---|---|
| Kernel architecture | OPEN |
| Moving the kernel to the higher half, and the layout of user address spaces | OPEN (D3) |
| Processes, scheduling and system calls | OPEN |
| Keyboard layout and a key repeat policy | OPEN |
| Drivers architecture | OPEN |
