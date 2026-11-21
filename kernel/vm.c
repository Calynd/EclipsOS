#include <stdint.h>
#include "memory.h"
#include "vm.h"
#include "vmm.h"

#define VM_IDENTITY_LIMIT (1ull << 30)

static struct vmm_space kernel_space;

static uint64_t vm_alloc_frame(void *context)
{
    (void)context;
    return memory_alloc_frame();
}

static void vm_free_frame(void *context, uint64_t physical)
{
    (void)context;
    memory_free_frame(physical);
}

static void *vm_map_physical(void *context, uint64_t physical)
{
    (void)context;
    return phys_to_virt(physical);
}

static void vm_flush(void *context, uint64_t virtual_address)
{
    (void)context;
    __asm__ volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");
}

static const struct vmm_ops kernel_ops = {
    .alloc_frame = vm_alloc_frame,
    .free_frame = vm_free_frame,
    .map_physical = vm_map_physical,
    .flush = vm_flush,
    .context = NULL,
};

int vm_init(void)
{
    if (vmm_space_init(&kernel_space, &kernel_ops) != VMM_OK)
        return -1;

    for (uint64_t address = 0; address < VM_IDENTITY_LIMIT; address += VMM_LARGE_PAGE_SIZE) {
        if (vmm_map_large(&kernel_space, address, address, VMM_WRITE) != VMM_OK)
            return -1;
    }

    __asm__ volatile("movq %0, %%cr3" : : "r"(kernel_space.root) : "memory");
    return 0;
}

struct vmm_space *vm_kernel_space(void)
{
    return &kernel_space;
}

int vm_self_test(void)
{
    uint64_t frame = memory_alloc_frame();
    volatile uint64_t *alias = (volatile uint64_t *)(uintptr_t)VM_TEST_ADDRESS;
    volatile uint64_t *direct;
    uint64_t physical = 0;
    uint64_t flags = 0;
    uint64_t removed = 0;

    if (frame == PMM_NO_FRAME)
        return 1;
    direct = phys_to_virt(frame);

    if (vmm_map(&kernel_space, VM_TEST_ADDRESS, frame, VMM_WRITE) != VMM_OK)
        return 2;
    if (vmm_translate(&kernel_space, VM_TEST_ADDRESS + 0x123, &physical, &flags) != VMM_OK)
        return 3;
    if (physical != frame + 0x123 || (flags & VMM_WRITE) == 0)
        return 3;

    alias[0] = 0xC0FFEEull;
    direct[1] = 0xBEEFull;
    if (direct[0] != 0xC0FFEEull || alias[1] != 0xBEEFull)
        return 4;

    if (vmm_map(&kernel_space, VM_TEST_ADDRESS, frame, VMM_WRITE) != VMM_ERR_EXISTS)
        return 5;
    if (vmm_unmap(&kernel_space, VM_TEST_ADDRESS, &removed) != VMM_OK || removed != frame)
        return 6;
    if (vmm_translate(&kernel_space, VM_TEST_ADDRESS, NULL, NULL) != VMM_ERR_NOT_MAPPED)
        return 7;
    if (vmm_translate(&kernel_space, 0xB8000, &physical, NULL) != VMM_OK || physical != 0xB8000)
        return 8;
    if (vmm_unmap(&kernel_space, 0x200000, NULL) != VMM_ERR_LARGE)
        return 9;

    if (memory_free_frame(frame) != 0)
        return 10;
    return 0;
}
