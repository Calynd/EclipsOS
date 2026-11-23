#include <stdint.h>
#include "gdt.h"
#include "tss.h"

struct gdt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static uint64_t gdt[8] = {
    0x0000000000000000ull,
    0x00AF9A000000FFFFull,
    0x00CF92000000FFFFull,
    0x00CFFA000000FFFFull,
    0x00CFF2000000FFFFull,
    0x00AFFA000000FFFFull,
    0x0000000000000000ull,
    0x0000000000000000ull,
};

static struct gdt_pointer gdt_pointer;

void gdt_init(void)
{
    uint64_t descriptor[2];

    tss_init();
    tss_make_descriptor((uint64_t)(uintptr_t)tss_get(), sizeof(struct tss) - 1, descriptor);
    gdt[6] = descriptor[0];
    gdt[7] = descriptor[1];

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

    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS) : "memory");
}
