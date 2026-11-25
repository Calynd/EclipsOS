#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "boot_tables.h"
#include "layout.h"
#include "vmm.h"

#define TABLES_PHYSICAL 0x10D000ull

static int failures;
static uint8_t *tables;

static void check(const char *name, int condition)
{
    if (!condition) {
        printf("FAIL %s\n", name);
        failures++;
    }
}

static void *map_physical(void *context, uint64_t physical)
{
    (void)context;
    if (physical < TABLES_PHYSICAL || physical >= TABLES_PHYSICAL + BOOT_TABLES_SIZE) {
        printf("FAIL table access outside the boot tables: 0x%llx\n", (unsigned long long)physical);
        failures++;
        exit(1);
    }
    return tables + (physical - TABLES_PHYSICAL);
}

static int translates(const struct vmm_space *space, uint64_t virtual_address, uint64_t expected)
{
    uint64_t physical = 0;

    return vmm_translate(space, virtual_address, &physical, NULL) == VMM_OK && physical == expected;
}

static int unmapped(const struct vmm_space *space, uint64_t virtual_address)
{
    return vmm_translate(space, virtual_address, NULL, NULL) == VMM_ERR_NOT_MAPPED;
}

int main(void)
{
    struct vmm_ops ops;
    struct vmm_space space;
    uint64_t *pml4;
    unsigned int present = 0;
    uint64_t flags = 0;

    tables = aligned_alloc(4096, BOOT_TABLES_SIZE);
    check("allocation", tables != NULL);
    memset(tables, 0xFF, BOOT_TABLES_SIZE);
    boot_build_tables((volatile uint64_t *)tables, TABLES_PHYSICAL);

    memset(&ops, 0, sizeof ops);
    ops.map_physical = map_physical;
    space.root = TABLES_PHYSICAL;
    space.ops = &ops;

    pml4 = (uint64_t *)tables;
    for (unsigned int i = 0; i < 512; i++) {
        if (pml4[i] & 1)
            present++;
    }
    check("exactly three top level entries", present == 3);
    check("entry 0 is the low map", (pml4[0] & 1) != 0);
    check("entry 256 is the direct map", (pml4[256] & 1) != 0);
    check("entry 511 is the kernel window", (pml4[511] & 1) != 0);

    check("identity start", translates(&space, 0x0, 0x0));
    check("identity kernel", translates(&space, 0x100000, 0x100000));
    check("identity video memory", translates(&space, 0xB8000, 0xB8000));
    check("identity boot information", translates(&space, 0x1000, 0x1000));
    check("identity last byte", translates(&space, 0x3FFFFFFF, 0x3FFFFFFF));
    check("identity ends at 1 GiB", unmapped(&space, 0x40000000));

    check("direct map start", translates(&space, HHDM_BASE, 0x0));
    check("direct map video memory", translates(&space, HHDM_BASE + 0xB8000, 0xB8000));
    check("direct map boot information", translates(&space, HHDM_BASE + 0x1000, 0x1000));
    check("direct map deep address", translates(&space, HHDM_BASE + 0x7FE0000, 0x7FE0000));
    check("direct map last byte", translates(&space, HHDM_BASE + 0x3FFFFFFF, 0x3FFFFFFF));
    check("direct map ends at 1 GiB", unmapped(&space, HHDM_BASE + 0x40000000));

    check("kernel window start", translates(&space, KERNEL_VIRT_BASE, 0x0));
    check("kernel entry", translates(&space, KERNEL_VIRT_BASE + KERNEL_PHYS_BASE, KERNEL_PHYS_BASE));
    check("kernel window deep address", translates(&space, KERNEL_VIRT_BASE + 0x20ABCDE, 0x20ABCDE));
    check("kernel window ends at 1 GiB", unmapped(&space, KERNEL_VIRT_BASE + 0x40000000));

    check("other low addresses are unmapped", unmapped(&space, 0x8000000000ull));
    check("other high addresses are unmapped", unmapped(&space, 0xFFFF900000000000ull));
    check("user half above the identity map is unmapped", unmapped(&space, 0x200000000ull));

    check("mappings are writable", vmm_translate(&space, HHDM_BASE + 0x1000, NULL, &flags) == VMM_OK &&
                                       (flags & VMM_WRITE) != 0);
    check("mappings are not user accessible", (flags & VMM_USER) == 0);

    check("constants agree with the page table indexes", ((HHDM_BASE >> 39) & 511) == 256 &&
                                                            ((KERNEL_VIRT_BASE >> 39) & 511) == 511 &&
                                                            ((KERNEL_VIRT_BASE >> 30) & 511) == 510);

    free(tables);
    if (failures == 0)
        printf("test_boot_tables: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
