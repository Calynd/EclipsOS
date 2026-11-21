#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vmm.h"

#define ARENA_FRAMES 256

static int failures;

struct arena {
    uint8_t *memory;
    uint8_t used[ARENA_FRAMES];
    unsigned int allocated;
    unsigned int flushes;
    unsigned int limit;
};

static void check(const char *name, int condition)
{
    if (!condition) {
        printf("FAIL %s\n", name);
        failures++;
    }
}

static uint64_t arena_alloc(void *context)
{
    struct arena *a = context;

    if (a->limit != 0 && a->allocated >= a->limit)
        return PMM_NO_FRAME;
    for (unsigned int i = 1; i < ARENA_FRAMES; i++) {
        if (!a->used[i]) {
            a->used[i] = 1;
            a->allocated++;
            memset(a->memory + (size_t)i * 4096, 0xAB, 4096);
            return (uint64_t)i * 4096;
        }
    }
    return PMM_NO_FRAME;
}

static void arena_free(void *context, uint64_t physical)
{
    struct arena *a = context;
    unsigned int index = (unsigned int)(physical / 4096);

    if (index == 0 || index >= ARENA_FRAMES || !a->used[index]) {
        printf("FAIL invalid or double free of frame %u\n", index);
        failures++;
        return;
    }
    a->used[index] = 0;
    a->allocated--;
}

static void *arena_map(void *context, uint64_t physical)
{
    struct arena *a = context;

    return a->memory + physical;
}

static void arena_flush(void *context, uint64_t virtual_address)
{
    struct arena *a = context;

    (void)virtual_address;
    a->flushes++;
}

int main(void)
{
    struct arena arena;
    struct vmm_ops ops;
    struct vmm_space space;
    uint64_t physical;
    uint64_t flags;

    memset(&arena, 0, sizeof arena);
    arena.memory = calloc(ARENA_FRAMES, 4096);
    check("arena", arena.memory != NULL);
    ops.alloc_frame = arena_alloc;
    ops.free_frame = arena_free;
    ops.map_physical = arena_map;
    ops.flush = arena_flush;
    ops.context = &arena;

    check("init", vmm_space_init(&space, &ops) == VMM_OK);
    check("root is one table", arena.allocated == 1);
    check("root is zeroed", ((uint64_t *)arena_map(&arena, space.root))[0] == 0 &&
                                ((uint64_t *)arena_map(&arena, space.root))[511] == 0);

    check("map first page", vmm_map(&space, 0x400000, 0x123000, VMM_WRITE) == VMM_OK);
    check("a path of 4 tables", arena.allocated == 4);
    check("translate", vmm_translate(&space, 0x400000, &physical, &flags) == VMM_OK && physical == 0x123000);
    check("translate keeps the page offset", vmm_translate(&space, 0x400ABC, &physical, &flags) == VMM_OK &&
                                                 physical == 0x123ABC);
    check("flags", (flags & VMM_WRITE) != 0 && (flags & VMM_USER) == 0);
    check("flushed on map", arena.flushes == 1);

    check("neighbor page needs no new table", vmm_map(&space, 0x401000, 0x124000, 0) == VMM_OK && arena.allocated == 4);
    check("neighbor is read only", vmm_translate(&space, 0x401000, &physical, &flags) == VMM_OK && physical == 0x124000 &&
                                       (flags & VMM_WRITE) == 0);

    check("remap fails", vmm_map(&space, 0x400000, 0x200000, VMM_WRITE) == VMM_ERR_EXISTS);
    check("remap kept the old target", vmm_translate(&space, 0x400000, &physical, NULL) == VMM_OK && physical == 0x123000);

    check("unaligned virtual", vmm_map(&space, 0x400800, 0x125000, 0) == VMM_ERR_INVALID);
    check("unaligned physical", vmm_map(&space, 0x402000, 0x125800, 0) == VMM_ERR_INVALID);
    check("non-canonical", vmm_map(&space, 0x0000800000000000ull, 0x125000, 0) == VMM_ERR_INVALID);
    check("physical beyond 52 bits", vmm_map(&space, 0x402000, 0x0010000000000000ull, 0) == VMM_ERR_INVALID);
    check("unknown flag", vmm_map(&space, 0x402000, 0x125000, 0x1) == VMM_ERR_INVALID);
    check("canonical high address", vmm_map(&space, 0xFFFF800000000000ull, 0x126000, VMM_WRITE) == VMM_OK);
    check("high address translates", vmm_translate(&space, 0xFFFF800000000123ull, &physical, NULL) == VMM_OK &&
                                         physical == 0x126123);

    unsigned int before = arena.allocated;
    check("page in another PT of the same PD", vmm_map(&space, 0x600000, 0x127000, 0) == VMM_OK &&
                                                   arena.allocated == before + 1);
    check("page in another PD", vmm_map(&space, 0x40000000, 0x128000, 0) == VMM_OK && arena.allocated == before + 3);
    check("page in another PDPT", vmm_map(&space, 0x8000000000ull, 0x129000, 0) == VMM_OK &&
                                      arena.allocated == before + 6);
    check("far page translates", vmm_translate(&space, 0x8000000000ull, &physical, NULL) == VMM_OK && physical == 0x129000);

    check("not mapped", vmm_translate(&space, 0x500000, NULL, NULL) == VMM_ERR_NOT_MAPPED);
    check("not mapped in an existing table", vmm_translate(&space, 0x402000, NULL, NULL) == VMM_ERR_NOT_MAPPED);
    check("not mapped far away", vmm_translate(&space, 0x123400000000ull, NULL, NULL) == VMM_ERR_NOT_MAPPED);

    unsigned int flushes = arena.flushes;
    check("unmap", vmm_unmap(&space, 0x400000, &physical) == VMM_OK && physical == 0x123000);
    check("flushed on unmap", arena.flushes == flushes + 1);
    check("unmapped", vmm_translate(&space, 0x400000, NULL, NULL) == VMM_ERR_NOT_MAPPED);
    check("neighbor survives", vmm_translate(&space, 0x401000, &physical, NULL) == VMM_OK && physical == 0x124000);
    check("unmap twice", vmm_unmap(&space, 0x400000, NULL) == VMM_ERR_NOT_MAPPED);
    check("unmap unaligned", vmm_unmap(&space, 0x400800, NULL) == VMM_ERR_INVALID);
    check("unmap in an empty region", vmm_unmap(&space, 0x123400000000ull, NULL) == VMM_ERR_NOT_MAPPED);
    check("map again after unmap", vmm_map(&space, 0x400000, 0x130000, VMM_WRITE) == VMM_OK);

    check("user page", vmm_map(&space, 0x700000, 0x131000, VMM_USER | VMM_WRITE) == VMM_OK);
    uint64_t *root = arena_map(&arena, space.root);
    check("user bit set along the path", (root[0] & VMM_USER) != 0);
    check("user flag reported", vmm_translate(&space, 0x700000, NULL, &flags) == VMM_OK && (flags & VMM_USER) != 0);
    check("no-execute flag stored", vmm_map(&space, 0x701000, 0x132000, VMM_NO_EXECUTE) == VMM_OK &&
                                        vmm_translate(&space, 0x701000, NULL, &flags) == VMM_OK &&
                                        (flags & VMM_NO_EXECUTE) != 0);

    check("large page", vmm_map_large(&space, 0x10000000, 0x200000, VMM_WRITE) == VMM_OK);
    check("large page translate start", vmm_translate(&space, 0x10000000, &physical, &flags) == VMM_OK &&
                                            physical == 0x200000 && (flags & VMM_WRITE) != 0);
    check("large page translate middle", vmm_translate(&space, 0x100ABCDE, &physical, NULL) == VMM_OK &&
                                             physical == 0x2ABCDE);
    check("large page translate inside", vmm_translate(&space, 0x100FFFFF, &physical, NULL) == VMM_OK &&
                                             physical == 0x2FFFFF);
    check("small page inside a large page fails", vmm_map(&space, 0x10001000, 0x133000, 0) == VMM_ERR_EXISTS);
    check("large page over a large page fails", vmm_map_large(&space, 0x10000000, 0x400000, 0) == VMM_ERR_EXISTS);
    check("unmap inside a large page", vmm_unmap(&space, 0x10001000, NULL) == VMM_ERR_LARGE);
    check("large page over a page table fails", vmm_map_large(&space, 0x400000, 0x400000, 0) == VMM_ERR_EXISTS);
    check("unaligned large page", vmm_map_large(&space, 0x10200000, 0x201000, 0) == VMM_ERR_INVALID);
    check("unaligned large virtual", vmm_map_large(&space, 0x10201000, 0x400000, 0) == VMM_ERR_INVALID);
    check("second large page", vmm_map_large(&space, 0x10200000, 0x400000, 0) == VMM_OK &&
                                   vmm_translate(&space, 0x10200010, &physical, NULL) == VMM_OK && physical == 0x400010);

    unsigned int tables = arena.allocated;
    vmm_space_destroy(&space);
    check("destroy frees every table", arena.allocated == 0 && tables > 6);
    check("root cleared", space.root == PMM_NO_FRAME);

    arena.allocated = 0;
    memset(arena.used, 0, sizeof arena.used);
    arena.limit = 2;
    check("init with room", vmm_space_init(&space, &ops) == VMM_OK);
    check("out of memory while building the path", vmm_map(&space, 0x400000, 0x123000, 0) == VMM_ERR_NO_MEMORY);
    check("failed map left nothing mapped", vmm_translate(&space, 0x400000, NULL, NULL) == VMM_ERR_NOT_MAPPED);
    arena.limit = 0;
    check("map works again with memory", vmm_map(&space, 0x400000, 0x123000, 0) == VMM_OK);
    vmm_space_destroy(&space);
    check("everything freed", arena.allocated == 0);

    memset(arena.used, 0, sizeof arena.used);
    arena.allocated = 0;
    arena.limit = 1;
    check("init uses the only frame", vmm_space_init(&space, &ops) == VMM_OK && arena.allocated == 1);
    check("a map with no frame left fails", vmm_map(&space, 0x400000, 0x123000, 0) == VMM_ERR_NO_MEMORY);
    vmm_space_destroy(&space);
    check("destroy after a failed map frees the root", arena.allocated == 0);

    arena.limit = 1;
    arena.allocated = 1;
    check("init fails without frames", vmm_space_init(&space, &ops) == VMM_ERR_NO_MEMORY);
    check("failed init leaves no root", space.root == PMM_NO_FRAME);
    vmm_space_destroy(&space);
    check("destroy of a failed space does nothing", arena.allocated == 1);
    free(arena.memory);

    if (failures == 0)
        printf("test_vmm: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
