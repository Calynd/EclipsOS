#include "heap.h"

#define BLOCK_FREE 0xF4EE1357u
#define BLOCK_USED 0xA110CA7Eu

struct heap_block {
    size_t size;
    uint32_t state;
    uint32_t reserved;
};

#define HEADER_SIZE sizeof(struct heap_block)

static struct heap_block *first_block(const struct heap *heap)
{
    return (struct heap_block *)heap->base;
}

static struct heap_block *next_block(const struct heap *heap, const struct heap_block *block)
{
    uint8_t *next = (uint8_t *)block + HEADER_SIZE + block->size;

    if (next >= heap->base + heap->size)
        return NULL;
    return (struct heap_block *)next;
}

int heap_init(struct heap *heap, void *base, size_t size)
{
    uintptr_t start = ((uintptr_t)base + HEAP_ALIGNMENT - 1) & ~(uintptr_t)(HEAP_ALIGNMENT - 1);
    size_t lost = start - (uintptr_t)base;
    struct heap_block *block;

    if (size < lost)
        return -1;
    size -= lost;
    size &= ~(size_t)(HEAP_ALIGNMENT - 1);
    if (size < HEADER_SIZE + HEAP_ALIGNMENT)
        return -1;

    heap->base = (uint8_t *)start;
    heap->size = size;
    block = first_block(heap);
    block->size = size - HEADER_SIZE;
    block->state = BLOCK_FREE;
    block->reserved = 0;
    return 0;
}

static void merge_free_blocks(struct heap *heap)
{
    struct heap_block *block = first_block(heap);

    while (block != NULL) {
        struct heap_block *next = next_block(heap, block);

        if (next != NULL && block->state == BLOCK_FREE && next->state == BLOCK_FREE) {
            block->size += HEADER_SIZE + next->size;
            continue;
        }
        block = next;
    }
}

void *heap_alloc(struct heap *heap, size_t size)
{
    struct heap_block *block;

    if (size == 0 || size > heap->size)
        return NULL;
    size = (size + HEAP_ALIGNMENT - 1) & ~(size_t)(HEAP_ALIGNMENT - 1);

    for (block = first_block(heap); block != NULL; block = next_block(heap, block)) {
        if (block->state != BLOCK_FREE || block->size < size)
            continue;

        if (block->size >= size + HEADER_SIZE + HEAP_ALIGNMENT) {
            struct heap_block *rest = (struct heap_block *)((uint8_t *)block + HEADER_SIZE + size);

            rest->size = block->size - size - HEADER_SIZE;
            rest->state = BLOCK_FREE;
            rest->reserved = 0;
            block->size = size;
        }
        block->state = BLOCK_USED;
        return (uint8_t *)block + HEADER_SIZE;
    }
    return NULL;
}

int heap_free(struct heap *heap, void *pointer)
{
    uint8_t *address = pointer;
    struct heap_block *block;

    if (address < heap->base + HEADER_SIZE || address >= heap->base + heap->size)
        return -1;
    if (((uintptr_t)address & (HEAP_ALIGNMENT - 1)) != 0)
        return -1;

    for (block = first_block(heap); block != NULL; block = next_block(heap, block)) {
        if ((uint8_t *)block + HEADER_SIZE != address)
            continue;
        if (block->state == BLOCK_FREE)
            return -2;
        if (block->state != BLOCK_USED)
            return -1;
        block->state = BLOCK_FREE;
        merge_free_blocks(heap);
        return 0;
    }
    return -1;
}

size_t heap_free_bytes(const struct heap *heap)
{
    size_t total = 0;

    for (struct heap_block *block = first_block(heap); block != NULL; block = next_block(heap, block)) {
        if (block->state == BLOCK_FREE)
            total += block->size;
    }
    return total;
}

size_t heap_used_bytes(const struct heap *heap)
{
    size_t total = 0;

    for (struct heap_block *block = first_block(heap); block != NULL; block = next_block(heap, block)) {
        if (block->state == BLOCK_USED)
            total += block->size;
    }
    return total;
}

int heap_check(const struct heap *heap)
{
    size_t covered = 0;

    for (struct heap_block *block = first_block(heap); block != NULL; block = next_block(heap, block)) {
        if (block->state != BLOCK_FREE && block->state != BLOCK_USED)
            return -1;
        if (block->size == 0 || (block->size & (HEAP_ALIGNMENT - 1)) != 0)
            return -1;
        covered += HEADER_SIZE + block->size;
        if (covered > heap->size)
            return -1;
    }
    return covered == heap->size ? 0 : -1;
}
