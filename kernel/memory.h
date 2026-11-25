#ifndef ECLIPS_MEMORY_H
#define ECLIPS_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "layout.h"
#include "pmm.h"
#include "monlod.h"

static inline void *phys_to_virt(uint64_t physical)
{
    return (void *)(uintptr_t)(HHDM_BASE + physical);
}

static inline uint64_t virt_to_phys(const void *virtual_address)
{
    uint64_t address = (uint64_t)(uintptr_t)virtual_address;

    if (address >= KERNEL_VIRT_BASE)
        return address - KERNEL_VIRT_BASE;
    return address - HHDM_BASE;
}

int memory_init(const struct monlod_info *info);
uint64_t memory_alloc_frame(void);
int memory_free_frame(uint64_t address);
uint64_t memory_free_frames(void);
size_t memory_heap_size(void);
void *kmalloc(size_t size);
void kfree(void *pointer);
int memory_self_test(void);

#endif
