# UDOS Documentation

UDOS (Unified Disk Operating System) is an independent operating system with its own architecture, formats and identity. It is not an MS-DOS clone and not a UNIX-like system.

Repository: https://github.com/Unified-Systems-Initiative/UDOS

## Status markers

Every statement in these documents carries one of these states:

| Marker | Meaning |
|---|---|
| **DECIDED** | Official project decision. Changing it requires prior discussion. |
| **PROPOSED** | Suggestion on the table, not yet an official decision. |
| **OPEN** | Not decided. Needs study and a decision. |

## Index

| Document | Content |
|---|---|
| [00-overview.md](00-overview.md) | Identity, components, directory layout, boot flow |
| [decisions.md](decisions.md) | Decision log and list of pending decisions |
| [customization.md](customization.md) | Replaceable components and the rules that keep them replaceable |
| [roadmap.md](roadmap.md) | Order of the work, and the decisions needed before each step |
| [specs/namespaces.md](specs/namespaces.md) | Namespace and resource addressing |
| [specs/udbl.md](specs/udbl.md) | Boot loader interface, boot stages, handoff to the kernel |
| [specs/udfs.md](specs/udfs.md) | UDFS on-disk format, tools and limits |
| [specs/memory-layout.md](specs/memory-layout.md) | Virtual and physical memory layout, entering the higher half |
| [specs/microkernel.md](specs/microkernel.md) | What the microkernel decision means, and the decisions it needs |
| [specs/abi.md](specs/abi.md) | System calls, user mode, segments and the task state segment |
| [specs/udk.md](specs/udk.md) | Kernel entry, build and current state |
| [specs/udsh.md](specs/udsh.md) | Shell prompt and command set |

## Building and testing

The Makefile is written for BSD make, `bmake` (**D-023**). It uses only features that GNU make also understands.

| Command | Purpose |
|---|---|
| `bmake` | Builds the tools, the kernel, UDBL and the disk image `build/udos.img` |
| `bmake run` | Boots the image in QEMU |
| `bmake check` | Checks the image with `udfsck` and compares the kernel inside it |
| `bmake test` | Runs the host tests |
| `bmake clean` | Removes `build/` |

Tools can be chosen on the command line, for example `bmake KERNEL_CC=gcc13 BOOT_CC=gcc13`. The variables are `KERNEL_CC`, `BOOT_CC`, `HOST_CC`, `LD` and `OBJCOPY`. Extra definitions for the kernel go in `KERNEL_DEFINES`, for example `bmake KERNEL_DEFINES=-DUDK_NULL_TEST run`, and for the user program in `USER_DEFINES`, for example `bmake USER_DEFINES=-DUSER_TEST=1 run`. Run `bmake clean` first, because changed definitions are not tracked.

Notes:

- Pattern rules, `$<` and the GNU functions (`wildcard`, `patsubst`, `shell`) are not portable between BSD make and GNU make, so every object has an explicit rule. Adding a kernel source file means adding it to `KERNEL_OBJS` and writing its rule, and adding a header means adding it to `KERNEL_HEADERS`.
- The kernel needs GCC (it uses GCC-only options) and the loader is written for the GNU assembler syntax.
- The Makefile was tested with GNU make 4.3 and GNU binutils. It was not run with bmake or with the LLVM linker and `objcopy` yet.
