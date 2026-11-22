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
| 5 | Virtual memory manager and own kernel page tables |
| 5b | Kernel in the higher half, direct map of physical memory, low addresses unmapped (ran in QEMU) |

## Next: processes, IPC and servers

**DECIDED (D-017).** UDSH is written only after processes exist. There is no temporary shell inside the kernel.

**DECIDED (D-022).** UDK is a microkernel. The consequences and the new decisions are in [specs/microkernel.md](specs/microkernel.md). The order below follows from it.

| Milestone | Content | Visible result | Needs decision |
|---|---|---|---|
| 6 | TSS, ring 3 and the system call entry, with a first system call that writes text | A hard-coded function runs in user mode and prints through the system call | D4 |
| 7 | UDE format, and a loader in the kernel for a UDE image embedded in the kernel | A program in the UDE format runs in its own address space | D6 |
| 8 | Processes, threads, the scheduler and exit | Two processes alternate on screen | D2 |
| 9 | IPC and capabilities | Two user processes exchange messages (ping and pong) | D5, D8 |
| 10 | Hardware access for user space: interrupt notifications, port access, device memory | The keyboard works from a server in user space and the in-kernel keyboard is removed | D9 |
| 11 | Boot modules: UDBL loads the programs listed in `boot.cfg`, and the kernel starts the first one | A server is replaced by editing `boot.cfg`, and the console moves to a server (the kernel keeps its panic output) | D10 |
| 12 | Storage servers: disk driver (ATA PIO), UDFS server and the namespace service | `disk:0//` paths resolve and a program is loaded from disk | |
| 13 | UDSH as a user program with `ver`, `help`, `run`, `go`, `dir` and the other commands, the user and uptime in the corner | The prompt `disk:0//>` | D7 (may be postponed) |

Milestones 6 to 9 change only the kernel image. The loader changes at milestone 11.

## Decisions needed

| ID | Decision | Options and consequences | Suggestion | Needed before |
|---|---|---|---|---|
| D1 | Kernel architecture | **DECIDED**, D-022: microkernel | | |
| D2 | Tasking model | **Single task:** one program at a time, like DOS. **Cooperative:** processes yield by themselves. **Preemptive:** the timer switches processes. | Preemptive round robin. The timer interrupt already exists, and user programs with permissions need isolation anyway. A single-task use is just one process. | 8 |
| D3 | Kernel memory layout | **DECIDED**, D-021: higher half | | |
| D4 | System call mechanism | **`syscall` instruction**, **software interrupt** (a vector) or **call gates**. | The `syscall` instruction, with the register convention written in the ABI specification. | 6 |
| D5 | How programs refer to resources | **Capabilities:** handles in a per-process table, received from the parent, with a namespace service that maps names such as `disk:0//` to them. **Handles opened by namespace path in the kernel:** the kernel resolves the names itself. | Capabilities, because IPC endpoints are capabilities in a microkernel. | 9 |
| D6 | UDE layout | Fixed load address versus relocatable code. Sizes of stack and zero-initialized data. Which header fields are mandatory. | Fixed load address at first, with a header field reserved for relocation. | 7 |
| D7 | Permission model | Users, `root`, and what is checked (files, namespaces, processes). The UDFS inode has 16 reserved bytes for it. | Postpone until UDSH works. | 13 |
| D8 | IPC model | See [specs/microkernel.md](specs/microkernel.md). | Synchronous call and reply, register messages, page sharing for bulk data, a separate asynchronous notification. | 9 |
| D9 | Hardware access for drivers | See [specs/microkernel.md](specs/microkernel.md). | Capabilities per resource. | 10 |
| D10 | How the first servers are loaded | See [specs/microkernel.md](specs/microkernel.md). | By the loader, from the modules in `boot.cfg`. | 11 |

UDOS must stay customizable (D-018). The contracts that each milestone creates are described in [customization.md](customization.md).

Other open items, such as the keyboard layout (US today) and whether file names ignore case, are in [decisions.md](decisions.md).
