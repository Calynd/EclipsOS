#ifndef ECLIPS_MONLOD_H
#define ECLIPS_MONLOD_H

#include <stdint.h>

#define MONLOD_MAGIC 0x444C4E4Du
#define MONLOD_INFO_VERSION 1
#define MONLOD_MMAP_USABLE 1

struct monlod_mmap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
};

struct monlod_info {
    uint32_t magic;
    uint16_t version;
    uint8_t boot_drive;
    uint8_t cursor_row;
    uint32_t mmap_count;
    uint32_t reserved;
    struct monlod_mmap_entry mmap[];
};

#endif
