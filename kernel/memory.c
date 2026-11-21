#include <stdint.h>
#include "heap.h"
#include "memory.h"
#include "pmm.h"

#define MEMORY_LOW_LIMIT 0x100000ull
#define MEMORY_LIMIT (1ull << 30)
#define MEMORY_FRAMES (MEMORY_LIMIT / PMM_FRAME_SIZE)
#define HEAP_BYTES (1u << 20)

extern char __kernel_start[];
extern char __kernel_end[];

static uint8_t frame_bitmap[MEMORY_FRAMES / 8];
static struct pmm pmm;
static struct heap heap;
static size_t heap_initial_free;

int memory_init(const struct udbl_info *info)
{
    uint64_t heap_physical;

    pmm_init(&pmm, frame_bitmap, MEMORY_FRAMES);

    for (uint32_t i = 0; i < info->mmap_count; i++) {
        const struct udbl_mmap_entry *entry = &info->mmap[i];
        uint64_t start = entry->base;
        uint64_t end = entry->base + entry->length;

        if (entry->type != UDBL_MMAP_USABLE || entry->length == 0)
            continue;
        if (end < start)
            end = UINT64_MAX;
        if (start < MEMORY_LOW_LIMIT)
            start = MEMORY_LOW_LIMIT;
        if (end > MEMORY_LIMIT)
            end = MEMORY_LIMIT;
        if (start < end)
            pmm_free_range(&pmm, start, end);
    }

    pmm_reserve_range(&pmm, virt_to_phys(__kernel_start), virt_to_phys(__kernel_end));

    heap_physical = pmm_alloc_contiguous(&pmm, HEAP_BYTES / PMM_FRAME_SIZE);
    if (heap_physical == PMM_NO_FRAME)
        return -1;
    if (heap_init(&heap, phys_to_virt(heap_physical), HEAP_BYTES) != 0)
        return -1;
    heap_initial_free = heap_free_bytes(&heap);
    return 0;
}

uint64_t memory_alloc_frame(void)
{
    return pmm_alloc(&pmm);
}

int memory_free_frame(uint64_t address)
{
    return pmm_free(&pmm, address);
}

uint64_t memory_free_frames(void)
{
    return pmm.free_count;
}

size_t memory_heap_size(void)
{
    return heap.size;
}

void *kmalloc(size_t size)
{
    return heap_alloc(&heap, size);
}

void kfree(void *pointer)
{
    heap_free(&heap, pointer);
}

int memory_self_test(void)
{
    uint64_t frames[3];
    uint64_t before;
    volatile uint64_t *words;
    uint8_t *small;
    uint8_t *large;

    before = memory_free_frames();
    for (int i = 0; i < 3; i++) {
        frames[i] = memory_alloc_frame();
        if (frames[i] == PMM_NO_FRAME)
            return 1;
        if ((frames[i] & (PMM_FRAME_SIZE - 1)) != 0)
            return 3;
        if (frames[i] < MEMORY_LOW_LIMIT || frames[i] >= MEMORY_LIMIT)
            return 4;
        if (frames[i] + PMM_FRAME_SIZE > virt_to_phys(__kernel_start) && frames[i] < virt_to_phys(__kernel_end))
            return 4;
    }
    if (frames[0] == frames[1] || frames[1] == frames[2] || frames[0] == frames[2])
        return 2;
    if (memory_free_frames() != before - 3)
        return 5;

    for (int i = 0; i < 3; i++) {
        words = phys_to_virt(frames[i]);
        words[0] = 0x1122334455667788ull ^ frames[i];
        words[511] = 0x8877665544332211ull ^ frames[i];
    }
    for (int i = 0; i < 3; i++) {
        words = phys_to_virt(frames[i]);
        if (words[0] != (0x1122334455667788ull ^ frames[i]) || words[511] != (0x8877665544332211ull ^ frames[i]))
            return 6;
    }

    for (int i = 0; i < 3; i++) {
        if (memory_free_frame(frames[i]) != 0)
            return 7;
    }
    if (memory_free_frames() != before)
        return 8;
    if (memory_free_frame(frames[0]) == 0)
        return 9;
    if (memory_free_frames() != before)
        return 8;

    small = kmalloc(100);
    large = kmalloc(5000);
    if (small == NULL || large == NULL)
        return 10;
    for (size_t i = 0; i < 100; i++)
        small[i] = (uint8_t)i;
    for (size_t i = 0; i < 5000; i++)
        large[i] = (uint8_t)(i * 7);
    for (size_t i = 0; i < 100; i++) {
        if (small[i] != (uint8_t)i)
            return 11;
    }
    for (size_t i = 0; i < 5000; i++) {
        if (large[i] != (uint8_t)(i * 7))
            return 11;
    }
    kfree(small);
    kfree(large);
    if (heap_check(&heap) != 0)
        return 12;
    if (heap_free_bytes(&heap) != heap_initial_free)
        return 12;
    return 0;
}
