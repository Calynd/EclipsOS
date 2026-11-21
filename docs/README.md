# UDOS Documentation

UDOS (Unified Disk Operating System) is an independent operating system with its own architecture, formats and identity. It is not an MS-DOS clone and not a UNIX-like system.

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
| [roadmap.md](roadmap.md) | Order of the work, and the decisions needed before each step |
| [specs/namespaces.md](specs/namespaces.md) | Namespace and resource addressing |
| [specs/udbl.md](specs/udbl.md) | Boot loader interface, boot stages, handoff to the kernel |
| [specs/udfs.md](specs/udfs.md) | UDFS on-disk format, tools and limits |
| [specs/udk.md](specs/udk.md) | Kernel entry, build and current state |
| [specs/udsh.md](specs/udsh.md) | Shell prompt and command set |

## Building and testing

| Command | Purpose |
|---|---|
| `make` | Builds the tools, the kernel, UDBL and the disk image `build/udos.img` |
| `make run` | Boots the image in QEMU |
| `make check` | Checks the image with `udfsck` and compares the kernel inside it |
| `make test` | Runs the host tests |
| `make clean` | Removes `build/` |
