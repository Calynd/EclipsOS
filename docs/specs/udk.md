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

## Current state (milestone 3)

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
| `kernel/io.h`, `kernel/interrupt.h` | Port I/O, interrupt frame and `sti`/`cli`/`hlt` helpers |

### Initialization order

1. Clear `.bss` and check the boot information.
2. Print boot drive and memory summary.
3. Load the kernel GDT and reload the segment registers.
4. Load the IDT and install the exception handlers.
5. Remap the PIC and mask every IRQ except the cascade.
6. Start the timer (100 Hz) and unmask IRQ 0.
7. Initialize the keyboard and unmask IRQ 1.
8. Enable interrupts.

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

`make test` builds and runs host tests for the logic that does not depend on the hardware: `tests/test_kbd_map.c` (translation, Shift, Caps Lock, release and extended codes), `tests/test_charbuf.c` (order, capacity, wraparound) and `tests/test_idt.c` (byte layout of an IDT entry).

## Known limits

- No TSS and no interrupt stack table. A stack overflow in a handler ends in a triple fault.
- Only the 8259 PIC is used. APIC and multiple processors are not supported.
- The keyboard code does not talk to the controller (no LEDs, no layout selection, no error handling).
- Exceptions are fatal. Nothing is recovered yet.
- Status: ran correctly in QEMU (GDT, IDT, PIC, timer with the uptime display, keyboard echo). The exception report has not been executed yet (`-DUDK_FAULT_TEST`). The code was also checked by disassembly (no SSE instructions, handler stack frames, GDT reload sequence) and by the host tests.

## Open items

| Item | Status |
|---|---|
| Kernel architecture | OPEN |
| Memory management (physical allocator, own page tables) | OPEN |
| Processes, scheduling and system calls | OPEN |
| Keyboard layout and a key repeat policy | OPEN |
| Drivers architecture | OPEN |
