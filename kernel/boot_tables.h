#ifndef ECLIPS_BOOT_TABLES_H
#define ECLIPS_BOOT_TABLES_H

#include <stdint.h>
#include "layout.h"

#define BOOT_TABLES_PAGES 5u
#define BOOT_TABLES_SIZE (BOOT_TABLES_PAGES * 0x1000u)

static inline __attribute__((always_inline)) void boot_build_tables(volatile uint64_t *tables, uint64_t physical)
{
    volatile uint64_t *pml4 = tables;
    volatile uint64_t *pdpt_low = tables + 512;
    volatile uint64_t *pdpt_direct = tables + 1024;
    volatile uint64_t *pdpt_kernel = tables + 1536;
    volatile uint64_t *pd = tables + 2048;

    for (unsigned int i = 0; i < BOOT_TABLES_PAGES * 512u; i++)
        tables[i] = 0;

    for (unsigned int i = 0; i < 512u; i++)
        pd[i] = ((uint64_t)i << 21) | 0x83ull;

    pdpt_low[0] = (physical + 4 * 0x1000ull) | 0x3ull;
    pdpt_direct[0] = (physical + 4 * 0x1000ull) | 0x3ull;
    pdpt_kernel[(KERNEL_VIRT_BASE >> 30) & 511u] = (physical + 4 * 0x1000ull) | 0x3ull;

    pml4[0] = (physical + 1 * 0x1000ull) | 0x3ull;
    pml4[(HHDM_BASE >> 39) & 511u] = (physical + 2 * 0x1000ull) | 0x3ull;
    pml4[(KERNEL_VIRT_BASE >> 39) & 511u] = (physical + 3 * 0x1000ull) | 0x3ull;
}

#endif
