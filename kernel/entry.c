#include <stdint.h>
#include "boot_tables.h"
#include "layout.h"

extern char __boot_tables_phys[];

__attribute__((noreturn, section(".boot.entry")))
void eclips_entry(const void *info)
{
    uint64_t tables = (uint64_t)(uintptr_t)__boot_tables_phys;
    uint64_t high_info = (uint64_t)(uintptr_t)info + HHDM_BASE;

    boot_build_tables((volatile uint64_t *)(uintptr_t)tables, tables);

    __asm__ volatile(
        "movq %0, %%cr3\n\t"
        "movabsq $__stack_top, %%rsp\n\t"
        "subq $8, %%rsp\n\t"
        "movabsq $eclips_main, %%rax\n\t"
        "jmp *%%rax"
        :
        : "r"(tables), "D"(high_info)
        : "rax", "memory");
    __builtin_unreachable();
}
