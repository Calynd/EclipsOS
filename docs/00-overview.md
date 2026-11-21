# UDOS Overview

## 1. Identity

**Name:** UDOS
**Full name:** Unified Disk Operating System

Principles (**DECIDED**):

- Not a clone of MS-DOS.
- Not UNIX-like by design.
- Own components, formats and conventions.
- Independent visual and technical identity, classic look in the spirit of MS-DOS.
- 100% independent from any other system. Other projects may only inspire ideas.
- Modular and organized development, thoroughly documented.

## 2. Components

| Acronym | Full name | Role | Status |
|---|---|---|---|
| UDBL | Unified Disk Boot Loader | Boot loader | DECIDED (name) |
| UDFS | Unified Disk File System | Native file system | DECIDED (name) |
| UDSH | Unified Disk Shell | Command interpreter | DECIDED (name) |
| UDE | Unified Disk Executable | Native executable format | DECIDED (name) |
| UDK | Unified Disk Kernel | Kernel (name only) | DECIDED (name) |
| udp | Unified Disk Packpage | Package manager | DEFERRED |

Component names are fixed and always written in uppercase. UDFS must never be replaced by UDF (Universal Disk Format).

Target platform (**DECIDED**): x86_64, BIOS firmware first. UEFI is considered only after the BIOS path works.

### 2.1 UDBL

Loads the kernel and transfers control to it. Interface: see [specs/udbl.md](specs/udbl.md).

### 2.2 UDFS

Native file system, with an own specification and independent implementation.

Items to define (**OPEN**): internal structure, metadata layout, directory structure, space allocation, maximum file size, permissions, links, journaling, crash recovery, compatibility with other file systems.

FAT support during early prototypes was discussed but is not a decision.

### 2.3 UDSH

Textual command interpreter. Prompt is based on the current namespace. Executes native commands and UDE programs and talks to the kernel through system services. Command set: see [specs/udsh.md](specs/udsh.md).

### 2.4 UDE

Native executable format. Proposed layout (**PROPOSED**, not a final binary specification):

```text
+--------------------------+
|       UDE HEADER         |
+--------------------------+
|    PROGRAM INFORMATION   |
+--------------------------+
|          CODE            |
+--------------------------+
|          DATA            |
+--------------------------+
|    OPTIONAL METADATA     |
+--------------------------+
```

Header fields under consideration: magic number `UDE\0`, format version, target architecture, flags, header size, entry point, code offset and size, data offset and size, memory requirements.

Principles (**DECIDED**):

- UDE is independent of UDFS.
- UDE programs must not depend exclusively on the native file system.
- The kernel controls program access to system resources.
- An ABI and a system call interface must be defined.

## 3. Directory layout

**DECIDED.** No directory may be added to the root without discussing its need and purpose.

```text
disk:0//
├── sys/
│   └── boot/
│       ├── udbl
│       ├── kernel
│       └── boot.cfg
├── bin/
├── usr/
│   └── root/
├── conf/
├── tmp/
└── dev/
```

| Directory | Purpose |
|---|---|
| `sys/` | Core system components |
| `sys/boot/` | UDBL, kernel and boot configuration |
| `bin/` | Native executables and commands |
| `usr/` | User directories |
| `usr/root/` | Home of the root user |
| `conf/` | System configuration |
| `tmp/` | Temporary files |
| `dev/` | Device representations and interfaces |

`boot.cfg` is a proposed file. Its format and content are **OPEN**.

## 4. Boot flow (conceptual)

1. UDBL starts.
2. Kernel is loaded.
3. Fundamental components are initialized.
4. Memory and drivers are initialized.
5. Processes are prepared.
6. Resources are made available.
7. Environment is initialized.
8. UDSH starts.

The real technical sequence, communication mechanisms and error handling are **OPEN**.
