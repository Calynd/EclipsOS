#include <stdint.h>
#include "gdt.h"

struct gdt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static uint64_t gdt[3] = {
    0x0000000000000000ull,
    0x00AF9A000000FFFFull,
    0x00CF92000000FFFFull,
};

static struct gdt_pointer gdt_pointer;

void gdt_init(void)
{
    gdt_pointer.limit = (uint16_t)(sizeof gdt - 1);
    gdt_pointer.base = (uint64_t)(uintptr_t)gdt;

    __asm__ volatile(
        "lgdt %0\n\t"
        "pushq %1\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n"
        "1:\n\t"
        "movw %2, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        :
        : "m"(gdt_pointer), "i"(GDT_KERNEL_CODE), "i"(GDT_KERNEL_DATA)
        : "rax", "memory");
}
