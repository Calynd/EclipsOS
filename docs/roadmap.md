# UDOS Roadmap

This document orders the work. The order is **PROPOSED**. The decisions that must be taken before each step are listed so that nothing is invented along the way.

## Working rules

- Each milestone ends in something that runs and can be seen or tested.
- Logic that does not depend on the hardware gets host tests (`make test`).
- Every decision goes to [decisions.md](decisions.md) before code depends on it.

## Fixed screen elements

**DECIDED (D-016).** The uptime display (`UP hh:mm:ss`) in the top right corner of the screen is never removed. Every console, shell or screen redesign keeps it.

**DECIDED (D-019).** The uptime display is part of UDSH. Until UDSH exists, UDK draws it. See [customization.md](customization.md).

**DECIDED (D-020).** The current user is shown in the same top right corner, and the prompt does not carry the user name. How the corner is shared between the user and the uptime is **OPEN**. A user model is needed for this, see D7 below.

## Done

| Milestone | Result |
|---|---|
| 1 | UDBL boot sector and `Boot>` prompt |
| 2 | Kernel in C, memory map, long mode |
| UDFS | UDFS v1, `mkudfs`, `udfsck`, kernel loaded by path from the file system, animated boot dots |
| 3 | GDT, IDT, exceptions, PIC, timer, keyboard, uptime display |
| 4 | Physical memory manager and kernel heap |
| 5 | Virtual memory manager and own kernel page tables (identity mapped, kernel not moved) |

## Next: processes and what they need

**DECIDED (D-017).** UDSH is written only after processes exist. There is no temporary shell inside the kernel.

| Milestone | Content | Visible result | Needs decision |
|---|---|---|---|
| 5b | Move the kernel to the higher half | `phys_to_virt` and the kernel link address change in one place each, and the lower half is free for programs | D3 |
| 6 | TSS, ring 3 and the system call entry | A hard-coded function runs in user mode and calls the kernel | D4 |
| 7 | UDE format and loader | A UDE image embedded in the kernel runs in user mode | D6 |
| 8 | Process table, scheduler and process exit | Two processes alternate on screen | D1, D2 |
| 9 | Disk driver (ATA PIO), UDFS reader in the kernel, namespace resolution | `disk:0//` paths resolve inside the kernel and a UDE is loaded from disk | D5 |
| 10 | UDSH as the first UDE program, with `ver`, `help`, `run`, `go`, `dir` and the other commands | The prompt `disk:0//>` | D7 (permissions, may be postponed) |

Console input and output for user programs (system calls to read the keyboard and write the screen) come with milestones 6 to 8.

## Decisions needed

| ID | Decision | Options and consequences | Suggestion | Needed before |
|---|---|---|---|---|
| D1 | Kernel architecture | **Monolithic:** drivers and file system in the kernel. Simpler and faster, a driver bug can bring the kernel down. **Microkernel:** drivers and file system run as processes. Needs IPC and a working scheduler before the first visible result, and much more work in the early milestones. | Monolithic, with a clear interface per subsystem so that parts can move out later. | 8 |
| D2 | Tasking model | **Single task:** one UDE at a time, like DOS. **Cooperative:** processes yield by themselves. **Preemptive:** the timer switches processes. | Preemptive round robin. The timer interrupt already exists, and user programs with permissions need isolation anyway. A single-task use is just one process. | 8 |
| D3 | Kernel memory layout | **Identity mapped at `0x100000` (today):** simple, but user programs cannot use the low addresses. **Higher half:** the kernel lives in the upper half of the address space and the lower half belongs to user programs. | Higher half. UDBL does not change, because the kernel builds its own page tables. The kernel needs a short entry path that runs at the low address until the new tables are active. | 5 |
| D4 | System call mechanism | **`syscall` instruction**, **software interrupt** (a vector) or **call gates**. | The `syscall` instruction, with the register convention written in the ABI specification. | 6 |
| D5 | How programs refer to resources | **Handles opened by namespace path:** the kernel resolves `disk:0//...`, `ram:0//...`, `net:0//...` and returns a handle. **Capabilities:** the program receives handles from its parent and cannot name anything else. | Handles opened by namespace path, with permissions checked by the kernel. | 9 |
| D6 | UDE layout | Fixed load address versus relocatable code. Sizes of stack and zero-initialized data. Which header fields are mandatory. | Fixed load address at first, with a header field reserved for relocation. | 7 |
| D7 | Permission model | Users, `root`, and what is checked (files, namespaces, processes). The UDFS inode has 16 reserved bytes for it. | Postpone until UDSH works. | 10 |

UDOS must stay customizable (D-018). The contracts that each milestone creates are described in [customization.md](customization.md).

Other open items, such as the keyboard layout (US today) and whether file names ignore case, are in [decisions.md](decisions.md).
