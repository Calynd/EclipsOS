# UDK as a Microkernel

**DECIDED (D-022).** UDK is a microkernel. The reason given by the project owner is customization: drivers and other services can be replaced without recompiling or replacing the kernel (see [../customization.md](../customization.md)).

Everything below is **PROPOSED** unless it says otherwise. It describes what the decision means for the code that exists today and for the work ahead.

## What stays in the kernel

| Part | Why |
|---|---|
| Physical memory manager and virtual memory manager | Only the kernel can program page tables. |
| Processes, threads and the scheduler | Needs the timer tick and the context switch. |
| IPC and the table of capabilities (handles) | It is the mechanism everything else is built on. |
| Interrupt entry and delivery of interrupts to user-space handlers; programming of the interrupt controller | Interrupts reach the kernel first. |
| The timer tick, used for scheduling and for time | Scheduling cannot depend on a server. |
| Handling of CPU exceptions | A fault in a process ends or reports that process. A fault in the kernel is fatal. |
| Minimal text output for panics and exception reports | A kernel that cannot say why it stopped is very hard to debug. It stays even when a console server exists. |
| Starting the first process | Someone has to start the servers. |

## What moves out of the kernel

| Part | Today | Becomes |
|---|---|---|
| Keyboard | `keyboard.c`, `kbd_map.c`, `charbuf.c` in the kernel | A keyboard server. The translation logic can be reused. |
| Screen console | `vga.c`, the echo loop and the status line | A console server. |
| Disk driver (ATA) | Does not exist yet | A storage server. |
| UDFS | Reader in UDBL, tools on the host | A file system server. |
| Namespaces `disk:`, `ram:`, `net:` | Not implemented | Services behind IPC. |
| UDSH and utilities | Not implemented | Ordinary programs (D-017, D-019). |

The keyboard, the console and the uptime display stay in the kernel until the servers that replace them work. The uptime display is never absent (D-016).

## Consequences

- **User mode comes first.** Without drivers in the kernel, nothing useful runs until the kernel can start user programs, switch between them and let them exchange messages. Ring 3, system calls, the UDE loader, the scheduler and IPC come before any driver moves out.
- **The system call interface is small.** It is mostly IPC, memory mapping, thread and process control, interrupt attachment and exit. This fits the `syscall` instruction (D4).
- **Drivers need hardware access.** A user-space driver needs I/O ports, interrupts, device memory and, for DMA, physical addresses. The kernel gives these as capabilities. Without an IOMMU a driver can still damage the system through DMA, so drivers are trusted components.
- **The first servers cannot come from a file system server.** A file system server cannot be loaded from the file system it provides. Either the loader or the kernel image carries the first servers (D10).
- **The loader contract grows.** If UDBL loads the servers, the boot information gains a list of modules. By rule R1 in [../customization.md](../customization.md) the structure changes by appending fields and raising its version.
- **Replacing a driver becomes replacing a program.** The entry in the boot configuration changes, and the kernel is not rebuilt. This is the benefit that motivated the decision.
- **The cost.** Each service call is a message exchange, and more code runs outside the kernel before the first visible result. The early milestones are larger than in a monolithic design.

## Decisions needed

| ID | Decision | Options and consequences | Suggestion | Needed before |
|---|---|---|---|---|
| D8 | IPC model | **Synchronous call and reply:** the sender waits. The kernel buffers nothing, messages are small and travel in registers, and large data moves by sharing pages. A server needs one thread per concurrent request. **Asynchronous queues:** the kernel buffers messages. More flexible, more kernel code and memory. | Synchronous call and reply with register messages and page sharing for bulk data, plus a separate asynchronous notification for interrupts. | 9 |
| D9 | Hardware access for drivers | **Capabilities per resource** (an IRQ line, a port range, a device memory range) given by the kernel to the first process and handed down. Port access by the I/O permission bitmap of the TSS, device memory by mapping physical ranges. **Open access** for any privileged server. | Capabilities per resource, handed out according to the boot configuration. | 10 |
| D10 | How the first servers are loaded | **By the loader:** UDBL reads the modules listed in `sys/boot/boot.cfg` and passes them to the kernel. Keeps everything replaceable by editing configuration. **In the kernel image:** like an initial RAM disk. Simple, but a change means rebuilding the kernel. **By the kernel from the file system:** needs a file system server first, so it cannot work for the first servers. | By the loader. It needs the format of `boot.cfg` and boot information version 2. | 11 |

Existing decisions that change in meaning:

| ID | Effect of D-022 |
|---|---|
| D2 (tasking model) | Still needed before milestone 8. The suggestion (preemptive round robin) stands. |
| D4 (system call mechanism) | Decided (D-024): the `syscall` instruction. See [abi.md](abi.md). |
| D5 (how programs refer to resources) | The natural answer is now capabilities, because IPC endpoints are capabilities. A namespace service maps names like `disk:0//` to them. |
| D6 (UDE layout) | Needed before the kernel can load the first servers, whoever carries them. |
| D7 (permission model) | Capabilities make part of the model. Users are an identity attached to a session (D-020), served by a service. Can wait until UDSH. |
