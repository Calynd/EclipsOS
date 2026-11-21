#ifndef UDK_UDBL_H
#define UDK_UDBL_H

#include <stdint.h>

#define UDBL_MAGIC 0x4C424455u
#define UDBL_INFO_VERSION 1
#define UDBL_MMAP_USABLE 1

struct udbl_mmap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
};

struct udbl_info {
    uint32_t magic;
    uint16_t version;
    uint8_t boot_drive;
    uint8_t cursor_row;
    uint32_t mmap_count;
    uint32_t reserved;
    struct udbl_mmap_entry mmap[];
};

#endif
