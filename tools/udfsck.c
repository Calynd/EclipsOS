#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "udfs.h"

static uint8_t *image;
static size_t image_sectors;
static const struct udfs_superblock *sb;
static unsigned int errors;
static uint8_t *used;
static uint32_t *refs;
static bool *visited;
static bool list_mode;

static void report(const char *format, ...)
{
    va_list args;

    fprintf(stderr, "udfsck: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    errors++;
}

static void fatal(const char *format, ...)
{
    va_list args;

    fprintf(stderr, "udfsck: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}

static const uint8_t *block_ptr(uint32_t block)
{
    return image + ((size_t)UDFS_VOLUME_START_LBA + block) * UDFS_BLOCK_SIZE;
}

static const struct udfs_inode *inode_ptr(uint32_t number)
{
    uint32_t index = number - 1;

    return (const struct udfs_inode *)(block_ptr(sb->inode_table_start + index / UDFS_INODES_PER_BLOCK)
                                       + (index % UDFS_INODES_PER_BLOCK) * UDFS_INODE_SIZE);
}

static bool inode_valid(uint32_t number)
{
    return number >= 1 && number <= sb->inode_count;
}

static void load_image(const char *path)
{
    FILE *f = fopen(path, "rb");
    long length;

    if (f == NULL)
        fatal("cannot open %s: %s", path, strerror(errno));
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length <= 0 || length % UDFS_BLOCK_SIZE != 0)
        fatal("%s: size is not a multiple of %u bytes", path, UDFS_BLOCK_SIZE);
    image_sectors = (size_t)length / UDFS_BLOCK_SIZE;
    if (image_sectors < UDFS_VOLUME_START_LBA + 1)
        fatal("%s: too small", path);
    image = malloc((size_t)length);
    if (image == NULL || fread(image, 1, (size_t)length, f) != (size_t)length)
        fatal("%s: read error", path);
    fclose(f);
}

static void check_superblock(void)
{
    sb = (const struct udfs_superblock *)block_ptr(0);

    if (sb->magic != UDFS_MAGIC)
        fatal("bad magic");
    if (sb->version_major != UDFS_VERSION_MAJOR)
        fatal("unsupported version %u.%u", sb->version_major, sb->version_minor);
    if (sb->block_size != UDFS_BLOCK_SIZE)
        fatal("unsupported block size %u", sb->block_size);
    if (sb->checksum != udfs_crc32(sb, UDFS_SB_CHECKSUM_OFFSET))
        report("superblock checksum mismatch");
    if (sb->total_blocks == 0 || (size_t)sb->total_blocks + UDFS_VOLUME_START_LBA > image_sectors)
        fatal("volume does not fit in the image");
    if (sb->bitmap_start == 0 || sb->bitmap_start + sb->bitmap_blocks > sb->inode_table_start)
        fatal("bad bitmap region");
    if (sb->bitmap_blocks * UDFS_BITS_PER_BLOCK < sb->total_blocks)
        fatal("bitmap too small");
    if (sb->inode_table_start + sb->inode_table_blocks > sb->data_start || sb->data_start > sb->total_blocks)
        fatal("bad inode table or data region");
    if (sb->inode_count == 0 || sb->inode_count > sb->inode_table_blocks * UDFS_INODES_PER_BLOCK)
        fatal("bad inode count");
    if (sb->root_inode != UDFS_ROOT_INODE)
        fatal("unexpected root inode %u", sb->root_inode);
}

static bool bit_is_set(const uint8_t *bitmap, uint32_t bit)
{
    return (bitmap[bit / 8] >> (bit % 8)) & 1u;
}

static void mark_extents(uint32_t number, const struct udfs_inode *inode)
{
    bool ended = false;
    uint64_t capacity = 0;

    for (unsigned int i = 0; i < UDFS_EXTENT_COUNT; i++) {
        const struct udfs_extent *e = &inode->extents[i];

        if (e->count == 0) {
            ended = true;
            if (e->start != 0)
                report("inode %u: empty extent %u with a start block", number, i);
            continue;
        }
        if (ended)
            report("inode %u: extent %u follows an empty extent", number, i);
        if (e->start < sb->data_start || (uint64_t)e->start + e->count > sb->total_blocks) {
            report("inode %u: extent %u is out of range", number, i);
            continue;
        }
        capacity += (uint64_t)e->count * UDFS_BLOCK_SIZE;
        for (uint32_t b = e->start; b < e->start + e->count; b++) {
            if (used[b])
                report("inode %u: block %u is used twice", number, b);
            used[b] = 1;
        }
    }
    if (capacity < inode->size)
        report("inode %u: size %llu exceeds the allocated space", number, (unsigned long long)inode->size);
}

static const struct udfs_dirent *dir_entry(const struct udfs_inode *dir, uint64_t index)
{
    uint64_t block_index = index / UDFS_DIRENTS_PER_BLOCK;
    uint64_t skipped = 0;

    for (unsigned int i = 0; i < UDFS_EXTENT_COUNT; i++) {
        const struct udfs_extent *e = &dir->extents[i];

        if (block_index < skipped + e->count)
            return (const struct udfs_dirent *)(block_ptr(e->start + (uint32_t)(block_index - skipped)))
                   + index % UDFS_DIRENTS_PER_BLOCK;
        skipped += e->count;
    }
    return NULL;
}

static void walk(uint32_t number, uint32_t parent, const char *path)
{
    const struct udfs_inode *inode = inode_ptr(number);

    if (visited[number]) {
        report("directory %u is reachable twice", number);
        return;
    }
    visited[number] = true;
    mark_extents(number, inode);

    if (inode->size % UDFS_DIRENT_SIZE != 0) {
        report("directory %u: size is not a multiple of the entry size", number);
        return;
    }

    uint64_t count = inode->size / UDFS_DIRENT_SIZE;
    bool has_dot = false;
    bool has_dotdot = false;

    for (uint64_t i = 0; i < count; i++) {
        const struct udfs_dirent *e = dir_entry(inode, i);

        if (e == NULL) {
            report("directory %u: entry %llu is outside the extents", number, (unsigned long long)i);
            return;
        }
        if (e->inode == 0)
            continue;
        if (!inode_valid(e->inode)) {
            report("directory %u: entry with invalid inode %u", number, e->inode);
            continue;
        }

        char name[UDFS_NAME_MAX + 1];
        memcpy(name, e->name, UDFS_NAME_MAX);
        name[UDFS_NAME_MAX] = 0;
        if (name[0] == 0) {
            report("directory %u: entry with an empty name", number);
            continue;
        }

        refs[e->inode]++;

        if (strcmp(name, ".") == 0) {
            has_dot = true;
            if (e->inode != number)
                report("directory %u: bad '.' entry", number);
            continue;
        }
        if (strcmp(name, "..") == 0) {
            has_dotdot = true;
            if (e->inode != parent)
                report("directory %u: bad '..' entry", number);
            continue;
        }

        const struct udfs_inode *child = inode_ptr(e->inode);
        char *child_path = malloc(strlen(path) + strlen(name) + 2);
        if (child_path == NULL)
            fatal("out of memory");
        sprintf(child_path, "%s/%s", path, name);

        if (list_mode)
            printf("%-5s %10llu  inode %-4u %s\n", child->type == UDFS_TYPE_DIR ? "dir" : "file",
                   (unsigned long long)child->size, e->inode, child_path);

        if (child->type == UDFS_TYPE_DIR) {
            walk(e->inode, number, child_path);
        } else if (child->type == UDFS_TYPE_FILE) {
            if (!visited[e->inode]) {
                visited[e->inode] = true;
                mark_extents(e->inode, child);
            }
        } else {
            report("inode %u: unknown type %u", e->inode, child->type);
        }
        free(child_path);
    }
    if (!has_dot || !has_dotdot)
        report("directory %u: missing '.' or '..'", number);
}

static void check_tree(void)
{
    used = calloc(sb->total_blocks, 1);
    refs = calloc((size_t)sb->inode_count + 1, sizeof *refs);
    visited = calloc((size_t)sb->inode_count + 1, sizeof *visited);
    if (used == NULL || refs == NULL || visited == NULL)
        fatal("out of memory");

    for (uint32_t b = 0; b < sb->data_start; b++)
        used[b] = 1;

    const struct udfs_inode *root = inode_ptr(UDFS_ROOT_INODE);
    if (root->type != UDFS_TYPE_DIR)
        fatal("root inode is not a directory");

    walk(UDFS_ROOT_INODE, UDFS_ROOT_INODE, "");

    uint32_t reachable = 0;
    uint32_t used_blocks = 0;
    for (uint32_t i = 1; i <= sb->inode_count; i++) {
        if (visited[i]) {
            reachable++;
            if (refs[i] != inode_ptr(i)->links)
                report("inode %u: link count %u, found %u references", i, inode_ptr(i)->links, refs[i]);
        } else if (inode_ptr(i)->type != 0) {
            report("inode %u is in use but not reachable", i);
        }
    }
    for (uint32_t b = 0; b < sb->total_blocks; b++) {
        if (used[b])
            used_blocks++;
        if (bit_is_set(block_ptr(sb->bitmap_start), b) != (used[b] != 0))
            report("block %u: bitmap disagrees with the file tree", b);
    }
    for (uint32_t b = sb->total_blocks; b < sb->bitmap_blocks * UDFS_BITS_PER_BLOCK; b++) {
        if (!bit_is_set(block_ptr(sb->bitmap_start), b)) {
            report("bitmap padding bit %u is clear", b);
            break;
        }
    }
    if (sb->free_blocks != sb->total_blocks - used_blocks)
        report("free block count is %u, expected %u", sb->free_blocks, sb->total_blocks - used_blocks);
    if (sb->free_inodes != sb->inode_count - reachable)
        report("free inode count is %u, expected %u", sb->free_inodes, sb->inode_count - reachable);

    printf("udfsck: volume \"%.16s\", version %u.%u, %u inodes (%u used), %u blocks (%u used)\n", sb->label,
           sb->version_major, sb->version_minor, sb->inode_count, reachable, sb->total_blocks, used_blocks);
}

static uint32_t lookup(uint32_t dir_number, const char *component, size_t length)
{
    const struct udfs_inode *dir = inode_ptr(dir_number);

    if (dir->type != UDFS_TYPE_DIR)
        return 0;
    for (uint64_t i = 0; i < dir->size / UDFS_DIRENT_SIZE; i++) {
        const struct udfs_dirent *e = dir_entry(dir, i);

        if (e == NULL)
            return 0;
        if (e->inode != 0 && strncmp(e->name, component, UDFS_NAME_MAX) == 0 && strlen(component) == length)
            return e->inode;
    }
    return 0;
}

static uint32_t resolve(const char *path)
{
    uint32_t current = UDFS_ROOT_INODE;

    while (*path == '/')
        path++;
    while (*path != 0) {
        const char *end = strchr(path, '/');
        size_t length = end ? (size_t)(end - path) : strlen(path);
        char component[UDFS_NAME_MAX + 2];

        if (length == 0 || length > UDFS_NAME_MAX)
            return 0;
        memcpy(component, path, length);
        component[length] = 0;
        current = lookup(current, component, length);
        if (current == 0)
            return 0;
        path += length;
        while (*path == '/')
            path++;
    }
    return current;
}

static void extract(const char *path)
{
    uint32_t number = resolve(path);

    if (number == 0)
        fatal("%s: not found", path);

    const struct udfs_inode *inode = inode_ptr(number);
    if (inode->type != UDFS_TYPE_FILE)
        fatal("%s: not a regular file", path);

    uint64_t remaining = inode->size;
    for (unsigned int i = 0; i < UDFS_EXTENT_COUNT && remaining > 0; i++) {
        uint64_t extent_bytes = (uint64_t)inode->extents[i].count * UDFS_BLOCK_SIZE;
        uint64_t chunk = remaining < extent_bytes ? remaining : extent_bytes;

        if (chunk > 0 && fwrite(block_ptr(inode->extents[i].start), 1, (size_t)chunk, stdout) != chunk)
            fatal("write error");
        remaining -= chunk;
    }
    if (remaining != 0)
        fatal("%s: extents are shorter than the file size", path);
}

static void show(const char *path)
{
    uint32_t number = resolve(path);

    if (number == 0)
        fatal("%s: not found", path);

    const struct udfs_inode *inode = inode_ptr(number);
    printf("inode %u, %s, %llu bytes, %u links\n", number, inode->type == UDFS_TYPE_DIR ? "directory" : "file",
           (unsigned long long)inode->size, inode->links);
    for (unsigned int i = 0; i < UDFS_EXTENT_COUNT; i++) {
        if (inode->extents[i].count != 0)
            printf("extent %u: start block %u (LBA %u), %u blocks\n", i, inode->extents[i].start,
                   inode->extents[i].start + UDFS_VOLUME_START_LBA, inode->extents[i].count);
    }
}

static void usage(void)
{
    fprintf(stderr, "usage: udfsck [-l] IMAGE\n       udfsck -x PATH IMAGE\n       udfsck -r PATH IMAGE\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *extract_path = NULL;
    const char *show_path = NULL;
    const char *image_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0)
            list_mode = true;
        else if (strcmp(argv[i], "-x") == 0 && i + 1 < argc)
            extract_path = argv[++i];
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
            show_path = argv[++i];
        else if (image_path == NULL)
            image_path = argv[i];
        else
            usage();
    }
    if (image_path == NULL)
        usage();

    load_image(image_path);
    check_superblock();

    if (extract_path != NULL) {
        extract(extract_path);
        return 0;
    }
    if (show_path != NULL) {
        show(show_path);
        return 0;
    }

    check_tree();
    if (errors != 0) {
        fprintf(stderr, "udfsck: %u error(s) found\n", errors);
        return 1;
    }
    printf("udfsck: clean\n");
    return 0;
}
