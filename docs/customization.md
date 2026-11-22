# Customization and Replaceable Components

**DECIDED (D-018).** UDOS is highly customizable. A person can replace each component: the boot loader, the shell and others. Replacing the boot loader is at the user's own risk.

## Rules

**PROPOSED.** These rules turn the decision into design constraints.

| Rule | Meaning |
|---|---|
| R1 | Every boundary between components is a documented contract with a version number. |
| R2 | A component depends only on the contract of another one, never on its internals or on its default implementation. |
| R3 | The default components are ordinary components. The kernel does not treat any of them in a special way. For example, it finds the first user program by configuration, not by its name. |
| R4 | UDOS guarantees the contracts, not the behavior of third-party components. |
| R5 | Whatever the standard system needs from a component is written in that component's specification, so that a replacement knows what it has to provide. |

## Components and their contracts

| Component | Replaced by | Contract | Contract status |
|---|---|---|---|
| UDBL | Any loader that can boot UDK | Entry conditions and boot information structure in [specs/udbl.md](specs/udbl.md). The structure has a version field. | Written, PROPOSED |
| UDK | Another kernel | Towards the loader, the same contract. Towards programs, the system call ABI. | Loader side written. ABI OPEN. |
| UDSH | Any program in the UDE format | How the kernel starts the first program, the system call ABI and the console interface | OPEN |
| UDFS | Another file system | On-disk format in [specs/udfs.md](specs/udfs.md). The driver interface inside the kernel depends on D1. | Format written, PROPOSED. Driver interface OPEN. |
| UDE | Another executable format | Binary layout | OPEN |
| Drivers and services | User-space servers (D-022). A driver is replaced by replacing a program and its entry in the boot configuration. | IPC protocol of each service and the hardware access interface | OPEN |
| Package manager | Deferred | | |

## Drivers without recompiling the kernel

**DECIDED (D-022).** UDK is a microkernel, so drivers (keyboard, console, disk) and services (file systems, namespaces) run as user programs. Replacing one means replacing a program and its entry in the boot configuration. The kernel is not rebuilt. What this needs is in [specs/microkernel.md](specs/microkernel.md): IPC, hardware access for drivers and a way to load the first servers.

## What a replacement boot loader must do

At the user's own risk, a loader other than UDBL must follow the handoff in [specs/udbl.md](specs/udbl.md): load the UDK flat binary at `0x100000`, enter it in 64-bit mode with interrupts disabled, `RDI` pointing to a boot information structure that carries the memory map, and a kernel stack as described there. Finding the kernel, for example by reading UDFS, is the loader's own job. Physical memory up to the end of the kernel image must be identity mapped; the kernel replaces the loader's page tables right away. The details are in [specs/memory-layout.md](specs/memory-layout.md).

## Uptime and the shell

**DECIDED (D-019).** The uptime display (`UP hh:mm:ss`, top right corner) is part of UDSH. While UDSH does not exist, UDK draws it (D-016). Consequences:

- When UDSH exists, UDK stops drawing it, so that it is never drawn twice.
- UDK has to offer user programs a way to write at a fixed screen position and to read the uptime. This belongs to the console interface and the system call ABI.
- A replacement shell is responsible for its own status line.
- **DECIDED (D-020).** The same corner shows the current user. The kernel is the authority on who the current user is, and UDK must offer a way to ask for it. What the status line shows is only information and never a security boundary, since a replacement shell can draw anything.

## Consequences for the next milestones

- The boot information structure grows only by appending fields, and the `version` field changes when the layout changes.
- The system call ABI and the UDE format must be specified before the first user program runs.
- The kernel starts the first user program from configuration. Where the configuration lives (`sys/boot/boot.cfg`, a file in `conf/` or another place) and its format are **OPEN**.
- The console interface must allow positioned output.
