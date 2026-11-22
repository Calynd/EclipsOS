# UDBL Interface

## Prompt

**DECIDED.** On startup UDBL shows only the prompt:

```text
Boot>
```

No banner, no version line and no instruction text.

## Commands

| Command | Description | Status |
|---|---|---|
| `start` | Boots the kernel, with or without flags | DECIDED (name), flags OPEN |

## Boot output after `start`

**PROPOSED** visual target. It is only the desired look, not a technical sequence.

```text
Boot> start

STARTING UDOS KERNEL..................OK
STARTING UDOS SYSTEM..................OK

INITIALIZING DRIVERS..................OK
INITIALIZING MEMORY...................OK
INITIALIZING PROCESSES................OK
MOUNTING SYSTEM RESOURCES.............OK
INITIALIZING ENVIRONMENT..............OK

WELCOME TO UDOS!

[SCREEN CLEARED]

disk:0//>
```

Only lines that report something that really happened are printed. Today that is the first line, printed by UDBL when the kernel has been read from disk.

**DECIDED (D-015).** Dots appear one per second on the same line, and `OK` is printed only after all the dots have appeared:

```text
STARTING UDOS KERNEL.
STARTING UDOS KERNEL..
STARTING UDOS KERNEL...
STARTING UDOS KERNEL..................OK
```

Each line above is the same screen line at a later moment.

**PROPOSED (P-007).** The number of dots is fixed per build (`BOOT_DOTS`, default 18). If the work finishes before the last dot, UDBL waits. If the work fails, the line ends with `FAILED` and the dots already shown stay.

## Platform

**DECIDED.** x86_64 CPU, BIOS firmware first. UEFI is considered only after the BIOS path works.

## Responsibilities

**DECIDED (D-014).** UDBL performs the switch to long mode and enters UDK already in 64-bit mode.

## Boot stages on BIOS

| Stage | Runs in | Role |
|---|---|---|
| Stage 1 | Real mode | The 512-byte boot sector loaded by the BIOS at `0x7C00`. Loads Stage 2 using INT 13h. |
| Stage 2 | Real mode, protected mode, long mode | Shows `Boot>`, parses the command line, loads the kernel, collects the memory map, enables A20, switches to long mode and jumps to the kernel. |

Consequences of the platform choice:

- Keyboard input (INT 16h), screen output and disk reads (INT 13h) exist only while the CPU is in real mode. Everything the BIOS must do happens before leaving real mode.
- Real mode reaches only 1 MiB directly. The kernel is read to a low buffer and copied above 1 MiB once the CPU is in protected mode.
- The BIOS passes the boot drive number in `DL`. **PROPOSED:** UDBL maps the boot drive to `disk:0`.

## Current implementation (milestone 2)

Sources: `boot/stage1.S`, `boot/stage2.S`. Build: `make`. Run: `make run`. Output: `build/udos.img`.

### Disk layout

**PROPOSED.** The disk holds a UDFS volume (see [udfs.md](udfs.md)).

| LBA | Content |
|---|---|
| 0 | Stage 1 |
| 1 to 8 | Stage 2 (`STAGE2_SECTORS` in the Makefile). The boot area allows up to 15 sectors. |
| 9 to 15 | Free, part of the boot area |
| 16 onward | UDFS volume. The kernel is the file `sys/boot/kernel`. |

### Stage 1

Normalizes `CS:IP` to `0000:7C00`, saves the boot drive, reads Stage 2 to `0x7E00` with INT 13h CHS (cylinder 0, head 0, sector 2) with 3 retries, and jumps to it. On failure prints `UDBL: DISK READ ERROR` and halts.

### Stage 2

| Step | Behavior |
|---|---|
| Prompt | Sets 80x25 text mode, shows `Boot> `, reads a line with INT 16h. Printable ASCII, Backspace and Enter. Input is lowercased. Maximum 63 characters. |
| `start` | Recognized with or without trailing flags. The flags pointer is stored but not used yet. |
| Load kernel | Prints `STARTING UDOS KERNEL`, then one dot per second driven by the BIOS tick counter at `0x46C` (about 18.2 ticks per second), checked between sectors. Reads the disk geometry with INT 13h AH=08, reads the UDFS superblock at LBA 16 and checks magic, major version and block size, then resolves the path `sys/boot/kernel` one component at a time through the inode table and the directories. The kernel file is read extent by extent, one sector per INT 13h call with LBA to CHS conversion and 3 retries, into the low buffer. After the load, waits with `HLT` until all `BOOT_DOTS` dots are shown, then prints `OK`. On failure prints `FAILED` and one error message. |
| Memory map | INT 15h AX=E820, up to 64 entries. |
| A20 | Checks A20 by wraparound test. Tries INT 15h AX=2401, then fast A20 on port `0x92`. |
| Boot information | Fills the boot information structure at `0x1000`. |
| Mode switch | Loads the GDT, enters protected mode, copies the kernel to `0x100000`, builds the page tables, enables PAE and long mode, enters 64-bit code and jumps to the kernel. |

Error messages:

| Message | Meaning |
|---|---|
| `UNKNOWN COMMAND` | Placeholder wording for unknown input |
| `UDBL: DISK READ ERROR` | Geometry query or sector read failed |
| `UDBL: BAD FILE SYSTEM` | Superblock not valid for UDBL, or an inode number out of range |
| `UDBL: KERNEL NOT FOUND` | A component of `sys/boot/kernel` is missing, or a non-final component is not a directory |
| `UDBL: KERNEL FILE INVALID` | `sys/boot/kernel` is not a regular file, is empty, is larger than 896 sectors or its extents are shorter than its size |
| `UDBL: MEMORY MAP ERROR` | E820 unsupported or returned no entries |
| `UDBL: A20 ERROR` | A20 could not be enabled |

After an error, UDBL returns to the `Boot>` prompt.

### Physical memory used

**PROPOSED.**

| Address | Use |
|---|---|
| `0x00500` to `0x07BFF` | Free. Boot information at `0x1000`, page tables at `0x2000` to `0x4FFF`. UDFS scratch buffers at `0x5000` (superblock), `0x5200` (inode block), `0x5400` (directory block) and `0x5600` (current inode). |
| `0x07C00` | Stage 1. Stack grows down from here during real mode. |
| `0x07E00` | Stage 2 (up to 4 KiB) |
| `0x10000` to `0x7FFFF` | Temporary kernel buffer (up to 896 sectors) |
| `0x80000` to `0x8FFFF` | Kernel boot stack, top at `0x8FFF8` |
| `0x100000` | Kernel load address and entry point |

### Paging at handoff

**PROPOSED.** Identity mapping of the first 1 GiB using 2 MiB pages (PML4 at `0x2000`, PDPT at `0x3000`, PD at `0x4000`). The kernel replaces these tables at its first instructions and runs in the higher half (see [memory-layout.md](memory-layout.md)). All it needs from them is an identity mapping of physical memory up to the end of its image and of the boot information.

### GDT

| Selector | Descriptor |
|---|---|
| `0x00` | Null |
| `0x08` | 32-bit code, flat |
| `0x10` | Data, flat |
| `0x18` | 64-bit code |

## Handoff to the kernel

**PROPOSED.**

The kernel is entered at `0x100000` in 64-bit mode, interrupts disabled, direction flag clear, with the System V AMD64 convention: `RDI` holds the address of the boot information structure and `RSP` is 8 modulo 16, as after a call.

Boot information structure at `0x1000`:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `magic`, the bytes `UDBL` (`0x4C424455`) |
| 4 | 2 | `version`, currently 1 |
| 6 | 1 | `boot_drive`, the BIOS drive number |
| 7 | 1 | `cursor_row`, text cursor row when the kernel is entered |
| 8 | 4 | `mmap_count`, number of memory map entries |
| 12 | 4 | `reserved` |
| 16 | 24 each | Memory map entries |

Memory map entry:

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | `base` |
| 8 | 8 | `length` |
| 16 | 4 | `type`, the E820 type (1 means usable) |
| 20 | 4 | `attributes`, E820 extended attributes |

The C definition is `kernel/udbl.h`.

## Known limits

- The kernel flat binary must fit the temporary buffer: at most 896 sectors (448 KiB). The Makefile and UDBL both check it.
- Stage 2 must fit `STAGE2_SECTORS` sectors and be readable from the first track after the boot sector: at most 17 sectors on a 1.44 MB floppy.
- Stage 1 uses fixed CHS addressing. Larger Stage 2 values need geometry-aware loading.
- UDBL reads UDFS with BIOS CHS addressing. Data beyond cylinder 1023 of the BIOS geometry is not reachable.
- UDBL reads at most 4 extents per file and only the first 4 extents of a directory.
- UDBL supports UDFS major version 1 with 512-byte blocks only.
- No bad-sector handling beyond retries.
- Dots are checked only between sectors and while waiting. A single BIOS call that blocks for more than a second delays the next dot.
- The tick counter is reset by the BIOS at midnight. If that happens during the dots, the remaining dots appear at once.
- Status: milestones 1 and 2, the animated dots and the loading of the kernel from UDFS ran correctly in QEMU (long mode entered, 7 memory map entries and about 128 MB usable reported by UDK). The failure cases (missing kernel, corrupt superblock, directory instead of file, oversized kernel, a directory spanning several blocks, a kernel in two extents and a kernel of the maximum size) were checked with an instruction-level emulation of the real mode code and have not been executed in QEMU.

## Open items

| Item | Status |
|---|---|
| BIOS or UEFI | BIOS decided, UEFI later |
| Boot flags | OPEN |
| Format and content of `boot.cfg` | OPEN |
| `boot.cfg` replacing the fixed kernel path | OPEN |
| Other UDBL commands | OPEN |
