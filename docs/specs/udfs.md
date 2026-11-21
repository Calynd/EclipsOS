# UDFS: Unified Disk File System

UDFS is the native file system of UDOS. The name is fixed (D-001). It must not be confused with UDF (Universal Disk Format).

Everything in this document is **PROPOSED** unless marked otherwise. It is a first design (version 1.0) meant to be small enough for a real mode boot loader to read, and to be extended later. UDFS is an independent design. It is not derived from any other file system, including the one of the KOREX project. From KOREX only the working method was reused: specification first, tools on the host, automated checks, then the driver.

## Goals of version 1

| Goal | Consequence |
|---|---|
| UDBL must find and read the kernel by path | Simple, fixed-size structures. No variable-length records. |
| Tools must run on the host | `mkudfs` creates volumes and `udfsck` verifies them, both in C. |
| Room to grow | Version fields, flags, reserved bytes and a block size field. |

Not in version 1 (**OPEN**): permissions, hard links between directories, journaling, crash recovery, timestamps, compatibility with other file systems, indirect extents, partitions, case folding.

## Conventions

- All integers are little-endian.
- A block is 512 bytes, the same as a BIOS sector.
- Block numbers are 32-bit and relative to the start of the volume. Block 0 is the superblock.
- One UDFS volume covers one disk (D-004: `disk:N` is a whole disk). Partitions are **OPEN**.

## Disk layout

| LBA | Content |
|---|---|
| 0 | UDBL Stage 1 (boot sector) |
| 1 to 15 | UDBL Stage 2 and free space of the boot area |
| 16 | Volume block 0: superblock |
| 16 + `bitmap_start` | Free space bitmap |
| 16 + `inode_table_start` | Inode table |
| 16 + `data_start` | Data blocks |

The boot area (LBA 0 to 15, 8 KiB) is outside the file system. A copy of it is stored as the file `sys/boot/udbl`, so that the loader can be updated from inside the system.

Layout produced by `mkudfs`:

| Region | Start block | Size |
|---|---|---|
| Superblock | 0 | 1 block |
| Bitmap | 1 | `ceil(total_blocks / 4096)` blocks |
| Inode table | after the bitmap | `inode_count / 8` blocks |
| Data | after the inode table | The rest |

## Superblock

Block 0 of the volume.

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 4 | `magic` | The bytes `UDFS` (`0x53464455`) |
| 4 | 2 | `version_major` | 1. A reader must refuse another major version. |
| 6 | 2 | `version_minor` | 0 |
| 8 | 4 | `block_size` | 512 in version 1 |
| 12 | 4 | `total_blocks` | Blocks in the volume, including the superblock |
| 16 | 4 | `inode_count` | Number of inodes in the table |
| 20 | 4 | `free_blocks` | Free data blocks |
| 24 | 4 | `free_inodes` | Free inodes |
| 28 | 4 | `bitmap_start` | First bitmap block |
| 32 | 4 | `bitmap_blocks` | Bitmap size in blocks |
| 36 | 4 | `inode_table_start` | First inode table block |
| 40 | 4 | `inode_table_blocks` | Inode table size in blocks |
| 44 | 4 | `data_start` | First data block |
| 48 | 4 | `root_inode` | Always 1 in version 1 |
| 52 | 4 | `flags` | 0 |
| 56 | 16 | `label` | Volume label, NUL padded |
| 72 | 436 | reserved | Zero |
| 508 | 4 | `checksum` | CRC-32 (IEEE, polynomial `0xEDB88320`) of bytes 0 to 507 |

The boot loader checks `magic`, `version_major` and `block_size`. It does not verify the checksum. `udfsck` does.

## Free space bitmap

One bit per block, starting at block 0. Bit `n` is byte `n / 8`, bit `n % 8` (least significant bit first). A set bit means used. Blocks of the metadata regions are always marked used. Padding bits after `total_blocks` in the last bitmap block are set.

## Inodes

64 bytes each. Inode numbers start at 1. Inode `n` is in table block `(n - 1) / 8`, at byte offset `((n - 1) % 8) * 64`. Inode number 0 means "no inode". An unused inode is all zero.

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 2 | `type` | 0 unused, 1 file, 2 directory |
| 2 | 2 | `flags` | 0 |
| 4 | 4 | `links` | Number of directory entries that reference the inode |
| 8 | 8 | `size` | Size in bytes |
| 16 | 16 | reserved | Zero. Space for timestamps and permissions (**OPEN**). |
| 32 | 32 | `extents` | 4 extents of 8 bytes |

Extent:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `start`, first block, relative to the volume |
| 4 | 4 | `count`, number of blocks. 0 means unused. |

Rules:

- Data of a file or directory is the concatenation of its extents, in order.
- Used extents come first. After an extent with `count` 0 all following extents are unused.
- An extent must lie inside the data region.
- The total size of the extents must be at least `size`.
- `mkudfs` always allocates a single contiguous extent. A file needs at most 4 extents in version 1.

For a directory, `links` is 2 plus the number of its subdirectories. For a file it is 1.

## Directories

A directory is an array of 32-byte entries stored in its data.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `inode`, 0 means a free entry |
| 4 | 28 | `name`, NUL padded. If the name has 28 bytes there is no terminator. |

Rules:

- A name has 1 to 28 bytes and does not contain `/` or NUL.
- Names are case sensitive.
- Every directory contains the entries `.` (itself) and `..` (its parent). For the root directory both refer to the root.
- `size` is a multiple of 32.
- Entries are stored in the order `.`, `..`, then the other names in ascending byte order. This order is produced by `mkudfs` and is not required of readers.

## Path resolution

A path is a sequence of names separated by `/`. Resolution starts at the root inode and looks up each name in the current directory, scanning its entries linearly. A name that is not found, or a non-final component that is not a directory, makes the lookup fail.

The namespace part of a path (`disk:0//`) is outside UDFS. How namespaces map to volumes is **OPEN** (see [namespaces.md](namespaces.md)).

## Requirements for booting

UDBL reads `sys/boot/kernel` from the volume at LBA 16 of the boot disk. For this to work:

- The superblock is valid for UDBL (magic, version 1, block size 512).
- The kernel is a regular file of at most 896 sectors (458752 bytes).
- The kernel uses at most 4 extents.
- The volume is reachable with BIOS CHS addressing, which limits the read area to cylinder 1023.

## Limits of version 1

| Item | Limit |
|---|---|
| Name length | 28 bytes |
| Extents per file | 4 |
| Volume size | 2 TiB (32-bit block numbers of 512 bytes) |
| File size | Bounded by 4 extents of at most 2 TiB |
| Free inodes | Fixed at creation. `mkudfs` creates at least 64 and twice the number it needs. |

## Tools

Both tools are in `tools/` and are built by `make`. They assume a little-endian host.

| Tool | Purpose |
|---|---|
| `build/tools/mkudfs --root DIR -o IMAGE [--boot-area FILE] [--size-kib N] [--label NAME]` | Creates an image. The tree of `DIR` becomes the content. `--boot-area` is copied to LBA 0 to 15 (at most 8 KiB). The default size is 1440 KiB. |
| `build/tools/udfsck IMAGE` | Checks the image: superblock and checksum, regions, inodes, extents, directories, `.` and `..`, link counts, overlap of blocks, bitmap and free counters. Exit status 0 means clean. |
| `build/tools/udfsck -l IMAGE` | Same check, and lists every file and directory. |
| `build/tools/udfsck -r PATH IMAGE` | Shows the inode and extents of a path. |
| `build/tools/udfsck -x PATH IMAGE` | Writes the content of a file to standard output. |

`make check` builds the image, runs `udfsck` and compares `sys/boot/kernel` inside the image with `build/kernel.bin`.

The tree stored in the image is built by the Makefile from the decided layout: `sys/boot`, `bin`, `usr/root`, `conf`, `tmp` and `dev`, plus the files `sys/boot/kernel` and `sys/boot/udbl`. The file `sys/boot/boot.cfg` is not created because its format is **OPEN**.

## Open items

| Item | Status |
|---|---|
| Permission model and the use of the reserved inode bytes | OPEN |
| Timestamps and their unit | OPEN |
| Hard links and symbolic links | OPEN |
| Journaling and crash recovery | OPEN |
| Case sensitivity versus a classic case-insensitive look | OPEN |
| Block sizes other than 512 | OPEN |
| Indirect extents for fragmented large files | OPEN |
| Partitions and several volumes on one disk | OPEN |
| FAT compatibility | OPEN |
| Online writing: allocation policy, directory growth and deletion in UDK | OPEN |
