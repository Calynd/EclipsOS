# System Calls and User Mode

**DECIDED (D-024).** Programs enter the kernel with the `syscall` instruction and return with `sysretq`.

Everything else in this document is **PROPOSED**. It is the first version of the interface, written for milestone 6 (see [../roadmap.md](../roadmap.md)). It will grow with the microkernel work (see [microkernel.md](microkernel.md)).

## Mechanism

The kernel enables the instruction and sets three model specific registers at start.

| Register | Value | Meaning |
|---|---|---|
| `EFER` | `SCE` bit set | Enables `syscall` and `sysretq`. |
| `STAR` | `0x0018_0008_0000_0000` | Bits 47 to 32 hold the kernel code selector (`0x08`), whose stack segment is the next one (`0x10`). Bits 63 to 48 hold the base for the return (`0x18`): the user stack segment is base plus 8 and the user code segment is base plus 16. |
| `LSTAR` | Address of `syscall_entry` | Where `syscall` jumps. |
| `SFMASK` | `0x40700` | Clears the trap, interrupt, direction and alignment check flags on entry. System calls run with interrupts masked. |

The entry code saves the user stack pointer, switches to the kernel trap stack, saves the registers into a frame, calls the C dispatcher and restores everything but the result. The frame layout is in `kernel/syscall.h`, and a host test checks its offsets.

## Register convention

| Register | In | Out |
|---|---|---|
| `RAX` | System call number | Result. A negative value is an error. |
| `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9` | Arguments 1 to 6 | Preserved |
| `RCX`, `R11` | | Destroyed. The hardware stores the return address and the flags in them. |
| Every other register | | Preserved |

## System calls

| Number | Name | Arguments | Result |
|---|---|---|---|
| 0 | `DEBUG_WRITE` | `RDI` pointer to text, `RSI` length (at most 4096) | The number of bytes, or an error. Writes printable characters and line feeds to the kernel console. Temporary, for diagnostics, until a console server exists. |
| 1 | `EXIT` | `RDI` exit code | Does not return. |
| 2 | `TIME` | none | Ticks of the system timer since start (100 per second). |

Errors (`include/udk_abi.h`):

| Value | Name | Meaning |
|---|---|---|
| -1 | `UDK_ERR_NO_SYSCALL` | Unknown system call number |
| -2 | `UDK_ERR_BAD_ADDRESS` | A user pointer is not valid |
| -3 | `UDK_ERR_INVALID` | An argument is not valid |

## User pointers

The kernel never reads user memory through the user pointer. It translates each page of the range with the address space tables, requires the page to be user accessible, and copies through the kernel mapping of the physical frame. So a bad pointer gives an error and cannot cause a page fault inside the kernel. A range is rejected when it starts at or above `0x0000800000000000`, when it wraps around, or when any page is missing or kernel only. The logic is in `kernel/uaccess.c` and has host tests.

## Segments

**PROPOSED.** The order of the user entries is dictated by how `sysretq` derives its segments from `STAR`.

| Selector | Descriptor |
|---|---|
| `0x00` | Null |
| `0x08` | Kernel code, 64-bit |
| `0x10` | Kernel data |
| `0x18` | User code, 32-bit. Not used. It only keeps the layout `sysretq` needs. |
| `0x20` (used as `0x23`) | User data |
| `0x28` (used as `0x2B`) | User code, 64-bit |
| `0x30` | Task state segment (takes two entries) |

## Task state segment

| Field | Value |
|---|---|
| `rsp0` | Top of the 16 KiB kernel trap stack. Interrupts and exceptions that arrive while a program runs use it. |
| `ist[0]` (IST 1) | Top of an 8 KiB stack used only by the double fault handler, so that a stack overflow is reported instead of resetting the machine. |
| I/O permission bitmap | None (the base points past the end). A program in user mode cannot access I/O ports. |

## Entering user mode

The kernel maps the program and a stack in the lower half, builds a return frame with the user data and code selectors, the stack pointer, the flags `0x202` (interrupts enabled) and the entry address, and executes `iretq`.

| Item | Value |
|---|---|
| Code | Mapped at `0x400000`, user accessible, not writable, one page per 4 KiB of the image |
| Stack | 8 KiB, top at `0x00007FFFFFFFF000`, user accessible and writable |
| Entry | First byte of the image (`_start`) |

## The first user program

`user/hello.c` is linked by `user/user.ld` at `0x400000` into a flat binary, and `kernel/user_image.S` embeds that binary in the kernel image. It writes two lines, reads the timer and exits. The linker script refuses a program with writable data or zero-initialized data, because only code and a stack are mapped. All this is replaced by the UDE loader (milestone 7).

When the program exits the kernel prints the exit code, unmaps and frees the pages, and goes on with the keyboard echo loop. The page tables created for the program stay allocated.

## Expected output

With the default QEMU memory, after `UDK: INTERRUPTS ENABLED`:

```text
UDK: STARTING USER PROGRAM
USER: HELLO FROM USER MODE
USER: UPTIME TICKS <n>
UDK: USER PROGRAM EXITED WITH CODE 0
UDK: USER MEMORY RELEASED, 32189 FRAMES FREE
```

The free frame count is 8 below the 32197 shown after the page tables: the self-test left 3 table frames and the user program 5 (three for the stack path and two for the code path). It can differ by a frame with another compiler. Earlier, a new line appears among the initialization lines: `UDK: SYSTEM CALL ENTRY READY, TSS LOADED, DOUBLE FAULT STACK SET`.

## Tests

Host tests (`bmake test`): `tests/test_descriptors.c` (frame and TSS layout, selector arithmetic for `sysretq`, the TSS descriptor, ABI constants) and `tests/test_uaccess.c` (copies across pages, rejected ranges, kernel only pages, wrap around).

To see the protection work, build the user program with a fault (run `bmake clean` first):

```text
bmake USER_DEFINES=-DUSER_TEST=1 run
bmake USER_DEFINES=-DUSER_TEST=2 run
bmake USER_DEFINES=-DUSER_TEST=3 run
```

| Test | What the program does | Expected report |
|---|---|---|
| 1 | Writes to address 0 | `EXCEPTION 14 PAGE FAULT`, `FAULT IN USER MODE`, `ERROR CODE 0x6`, `FAULT ADDRESS 0x0`, `PAGE NOT PRESENT, WRITE, USER` |
| 2 | Reads the kernel at `0xFFFFFFFF80100000` | `EXCEPTION 14 PAGE FAULT`, `FAULT IN USER MODE`, `ERROR CODE 0x5`, `FAULT ADDRESS 0xFFFFFFFF80100000`, `PROTECTION VIOLATION, READ, USER` |
| 3 | Executes `hlt` | `EXCEPTION 13 GENERAL PROTECTION FAULT`, `FAULT IN USER MODE`, `ERROR CODE 0x0` |

## Limits

- The program shares the kernel address space. Separate address spaces per process come with milestones 7 and 8.
- A fault in user mode still stops the whole system. Ending only the faulting program needs processes.
- System calls run with interrupts masked, and there is no scheduling.
- Only one program, embedded in the kernel, can run, and only once.
- `DEBUG_WRITE` writes to the kernel console and is not part of the final interface.
- Code and data pages are not marked no-execute yet.
