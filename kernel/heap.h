#ifndef ECLIPS_HEAP_H
#define ECLIPS_HEAP_H

#include <stddef.h>
#include <stdint.h>

#define HEAP_ALIGNMENT 16u

struct heap {
    uint8_t *base;
    size_t size;
};

int heap_init(struct heap *heap, void *base, size_t size);
void *heap_alloc(struct heap *heap, size_t size);
int heap_free(struct heap *heap, void *pointer);
size_t heap_free_bytes(const struct heap *heap);
size_t heap_used_bytes(const struct heap *heap);
int heap_check(const struct heap *heap);

#endif
