#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "udfs.h"

struct node {
    char *name;
    char *path;
    uint16_t type;
    uint32_t inode;
    uint32_t parent_inode;
    uint64_t size;
    uint32_t links;
    uint32_t first_block;
    uint32_t block_count;
    struct node **children;
    size_t child_count;
};

static struct node **nodes;
static size_t node_count;
static size_t node_capacity;

static void die(const char *format, ...)
{
    va_list args;

    fprintf(stderr, "mkudfs: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}

static void *xcalloc(size_t count, size_t size)
{
    void *p = calloc(count, size);

    if (p == NULL)
        die("out of memory");
    return p;
}

static char *xstrdup(const char *s)
{
    char *copy = strdup(s);

    if (copy == NULL)
        die("out of memory");
    return copy;
}

static char *join_path(const char *dir, const char *name)
{
    size_t length = strlen(dir) + strlen(name) + 2;
    char *path = xcalloc(length, 1);

    snprintf(path, length, "%s/%s", dir, name);
    return path;
}

static void register_node(struct node *n)
{
    if (node_count == node_capacity) {
        node_capacity = node_capacity ? node_capacity * 2 : 64;
        nodes = realloc(nodes, node_capacity * sizeof *nodes);
        if (nodes == NULL)
            die("out of memory");
    }
    nodes[node_count++] = n;
    n->inode = (uint32_t)node_count;
}

static int compare_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static struct node *scan(const char *path, const char *name, uint32_t parent_inode)
{
    struct stat st;
    struct node *n;

    if (stat(path, &st) != 0)
        die("cannot stat %s: %s", path, strerror(errno));

    n = xcalloc(1, sizeof *n);
    n->name = xstrdup(name);
    n->path = xstrdup(path);
    register_node(n);

    if (S_ISREG(st.st_mode)) {
        n->type = UDFS_TYPE_FILE;
        n->size = (uint64_t)st.st_size;
        n->links = 1;
        return n;
    }

    if (!S_ISDIR(st.st_mode))
        die("%s is neither a regular file nor a directory", path);

    n->type = UDFS_TYPE_DIR;
    n->parent_inode = parent_inode ? parent_inode : n->inode;

    DIR *dir = opendir(path);
    if (dir == NULL)
        die("cannot open %s: %s", path, strerror(errno));

    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        size_t length = strlen(entry->d_name);
        if (length == 0 || length > UDFS_NAME_MAX)
            die("name too long or empty in %s: %s", path, entry->d_name);
        if (count == capacity) {
            capacity = capacity ? capacity * 2 : 16;
            names = realloc(names, capacity * sizeof *names);
            if (names == NULL)
                die("out of memory");
        }
        names[count++] = xstrdup(entry->d_name);
    }
    closedir(dir);

    qsort(names, count, sizeof *names, compare_names);

    n->children = xcalloc(count ? count : 1, sizeof *n->children);
    n->child_count = count;
    n->links = 2;

    for (size_t i = 0; i < count; i++) {
        char *child_path = join_path(path, names[i]);
        n->children[i] = scan(child_path, names[i], n->inode);
        if (n->children[i]->type == UDFS_TYPE_DIR)
            n->links++;
        free(child_path);
        free(names[i]);
    }
    free(names);

    n->size = (uint64_t)(count + 2) * UDFS_DIRENT_SIZE;
    return n;
}

static uint32_t blocks_for(uint64_t size)
{
    return (uint32_t)((size + UDFS_BLOCK_SIZE - 1) / UDFS_BLOCK_SIZE);
}

static uint32_t round_up(uint32_t value, uint32_t multiple)
{
    return (value + multiple - 1) / multiple * multiple;
}

static void set_bit(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] = (uint8_t)(bitmap[bit / 8] | (1u << (bit % 8)));
}

static uint8_t *block_at(uint8_t *image, uint32_t block)
{
    return image + ((size_t)UDFS_VOLUME_START_LBA + block) * UDFS_BLOCK_SIZE;
}

static void write_directory(uint8_t *image, const struct node *dir, const struct node *parent)
{
    struct udfs_dirent *entries = (struct udfs_dirent *)block_at(image, dir->first_block);
    size_t index = 0;

    entries[index].inode = dir->inode;
    strncpy(entries[index].name, ".", UDFS_NAME_MAX);
    index++;
    entries[index].inode = parent->inode;
    strncpy(entries[index].name, "..", UDFS_NAME_MAX);
    index++;
    for (size_t i = 0; i < dir->child_count; i++) {
        entries[index].inode = dir->children[i]->inode;
        strncpy(entries[index].name, dir->children[i]->name, UDFS_NAME_MAX);
        index++;
    }
}

static void usage(void)
{
    fprintf(stderr, "usage: mkudfs --root DIR -o IMAGE [--boot-area FILE] [--size-kib N] [--label NAME]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *root_dir = NULL;
    const char *output = NULL;
    const char *boot_area = NULL;
    const char *label = "EclipsOS";
    unsigned long size_kib = 1440;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc)
            root_dir = argv[++i];
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (strcmp(argv[i], "--boot-area") == 0 && i + 1 < argc)
            boot_area = argv[++i];
        else if (strcmp(argv[i], "--label") == 0 && i + 1 < argc)
            label = argv[++i];
        else if (strcmp(argv[i], "--size-kib") == 0 && i + 1 < argc)
            size_kib = strtoul(argv[++i], NULL, 10);
        else
            usage();
    }
    if (root_dir == NULL || output == NULL || size_kib == 0)
        usage();
    if (strlen(label) >= UDFS_LABEL_SIZE)
        die("label must have at most %u characters", UDFS_LABEL_SIZE - 1);

    struct node *root = scan(root_dir, "", 0);

    uint32_t total_sectors = (uint32_t)(size_kib * 1024 / UDFS_BLOCK_SIZE);
    if (total_sectors <= UDFS_VOLUME_START_LBA + 8)
        die("image too small");

    uint32_t total_blocks = total_sectors - UDFS_VOLUME_START_LBA;
    uint32_t inode_count = round_up((uint32_t)node_count * 2, UDFS_INODES_PER_BLOCK);
    if (inode_count < 64)
        inode_count = 64;
    uint32_t inode_table_blocks = inode_count / UDFS_INODES_PER_BLOCK;
    uint32_t bitmap_blocks = (total_blocks + UDFS_BITS_PER_BLOCK - 1) / UDFS_BITS_PER_BLOCK;
    uint32_t bitmap_start = 1;
    uint32_t inode_table_start = bitmap_start + bitmap_blocks;
    uint32_t data_start = inode_table_start + inode_table_blocks;

    uint32_t next_block = data_start;
    for (size_t i = 0; i < node_count; i++) {
        struct node *n = nodes[i];
        n->block_count = blocks_for(n->size);
        if (n->block_count > 0) {
            if ((uint64_t)next_block + n->block_count > total_blocks)
                die("image too small for the contents of %s", root_dir);
            n->first_block = next_block;
            next_block += n->block_count;
        }
    }

    uint8_t *image = xcalloc(total_sectors, UDFS_BLOCK_SIZE);

    if (boot_area != NULL) {
        FILE *f = fopen(boot_area, "rb");
        if (f == NULL)
            die("cannot open %s: %s", boot_area, strerror(errno));
        size_t read = fread(image, 1, (size_t)UDFS_BOOT_AREA_SECTORS * UDFS_BLOCK_SIZE + 1, f);
        fclose(f);
        if (read > (size_t)UDFS_BOOT_AREA_SECTORS * UDFS_BLOCK_SIZE)
            die("boot area is larger than %u sectors", UDFS_BOOT_AREA_SECTORS);
    }

    uint8_t *bitmap = block_at(image, bitmap_start);
    for (uint32_t b = 0; b < next_block; b++)
        set_bit(bitmap, b);
    for (uint32_t b = total_blocks; b < bitmap_blocks * UDFS_BITS_PER_BLOCK; b++)
        set_bit(bitmap, b);

    for (size_t i = 0; i < node_count; i++) {
        struct node *n = nodes[i];
        uint32_t index = n->inode - 1;
        struct udfs_inode *inode = (struct udfs_inode *)(block_at(image, inode_table_start + index / UDFS_INODES_PER_BLOCK)
                                    + (index % UDFS_INODES_PER_BLOCK) * UDFS_INODE_SIZE);
        inode->type = n->type;
        inode->links = n->links;
        inode->size = n->size;
        if (n->block_count > 0) {
            inode->extents[0].start = n->first_block;
            inode->extents[0].count = n->block_count;
        }

        if (n->type == UDFS_TYPE_FILE && n->size > 0) {
            FILE *f = fopen(n->path, "rb");
            if (f == NULL)
                die("cannot open %s: %s", n->path, strerror(errno));
            size_t read = fread(block_at(image, n->first_block), 1, (size_t)n->size, f);
            fclose(f);
            if (read != n->size)
                die("short read on %s", n->path);
        } else if (n->type == UDFS_TYPE_DIR) {
            const struct node *parent = n == root ? root : nodes[n->parent_inode - 1];
            write_directory(image, n, parent);
        }
    }

    struct udfs_superblock *sb = (struct udfs_superblock *)block_at(image, 0);
    sb->magic = UDFS_MAGIC;
    sb->version_major = UDFS_VERSION_MAJOR;
    sb->version_minor = UDFS_VERSION_MINOR;
    sb->block_size = UDFS_BLOCK_SIZE;
    sb->total_blocks = total_blocks;
    sb->inode_count = inode_count;
    sb->free_blocks = total_blocks - next_block;
    sb->free_inodes = inode_count - (uint32_t)node_count;
    sb->bitmap_start = bitmap_start;
    sb->bitmap_blocks = bitmap_blocks;
    sb->inode_table_start = inode_table_start;
    sb->inode_table_blocks = inode_table_blocks;
    sb->data_start = data_start;
    sb->root_inode = UDFS_ROOT_INODE;
    sb->flags = 0;
    strncpy(sb->label, label, UDFS_LABEL_SIZE);
    sb->checksum = udfs_crc32(sb, UDFS_SB_CHECKSUM_OFFSET);

    FILE *out = fopen(output, "wb");
    if (out == NULL)
        die("cannot create %s: %s", output, strerror(errno));
    if (fwrite(image, UDFS_BLOCK_SIZE, total_sectors, out) != total_sectors)
        die("write error on %s", output);
    fclose(out);

    printf("mkudfs: %s: %zu inodes used of %u, %u blocks used of %u\n", output, node_count, inode_count,
           next_block, total_blocks);
    return 0;
}
