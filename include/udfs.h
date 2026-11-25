#ifndef ECLIPSOS_UDFS_H
#define ECLIPSOS_UDFS_H

#include <stddef.h>
#include <stdint.h>

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error UDFS version 1 structures require a little-endian host
#endif

#define UDFS_MAGIC 0x53464455u
#define UDFS_VERSION_MAJOR 1u
#define UDFS_VERSION_MINOR 0u
#define UDFS_BLOCK_SIZE 512u
#define UDFS_VOLUME_START_LBA 16u
#define UDFS_BOOT_AREA_SECTORS 16u
#define UDFS_INODE_SIZE 64u
#define UDFS_DIRENT_SIZE 32u
#define UDFS_NAME_MAX 28u
#define UDFS_LABEL_SIZE 16u
#define UDFS_EXTENT_COUNT 4u
#define UDFS_ROOT_INODE 1u
#define UDFS_TYPE_FILE 1u
#define UDFS_TYPE_DIR 2u
#define UDFS_INODES_PER_BLOCK (UDFS_BLOCK_SIZE / UDFS_INODE_SIZE)
#define UDFS_DIRENTS_PER_BLOCK (UDFS_BLOCK_SIZE / UDFS_DIRENT_SIZE)
#define UDFS_BITS_PER_BLOCK (UDFS_BLOCK_SIZE * 8u)
#define UDFS_SB_CHECKSUM_OFFSET 508u

struct udfs_superblock {
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_count;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t bitmap_start;
    uint32_t bitmap_blocks;
    uint32_t inode_table_start;
    uint32_t inode_table_blocks;
    uint32_t data_start;
    uint32_t root_inode;
    uint32_t flags;
    char label[UDFS_LABEL_SIZE];
    uint8_t reserved[436];
    uint32_t checksum;
} __attribute__((packed));

struct udfs_extent {
    uint32_t start;
    uint32_t count;
} __attribute__((packed));

struct udfs_inode {
    uint16_t type;
    uint16_t flags;
    uint32_t links;
    uint64_t size;
    uint8_t reserved[16];
    struct udfs_extent extents[UDFS_EXTENT_COUNT];
} __attribute__((packed));

struct udfs_dirent {
    uint32_t inode;
    char name[UDFS_NAME_MAX];
} __attribute__((packed));

_Static_assert(sizeof(struct udfs_superblock) == UDFS_BLOCK_SIZE, "superblock size");
_Static_assert(sizeof(struct udfs_inode) == UDFS_INODE_SIZE, "inode size");
_Static_assert(sizeof(struct udfs_dirent) == UDFS_DIRENT_SIZE, "directory entry size");

static inline uint32_t udfs_crc32(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

#endif
