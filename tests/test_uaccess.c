#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uaccess.h"
#include "vmm.h"

#define ARENA_FRAMES 256
#define DATA_FRAME_A 0x80000ull
#define DATA_FRAME_B 0x81000ull

static int failures;

struct arena {
    uint8_t *memory;
    uint8_t used[ARENA_FRAMES];
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

    for (unsigned int i = 1; i < 128; i++) {
        if (!a->used[i]) {
            a->used[i] = 1;
            memset(a->memory + (size_t)i * 4096, 0, 4096);
            return (uint64_t)i * 4096;
        }
    }
    return PMM_NO_FRAME;
}

static void arena_free(void *context, uint64_t physical)
{
    struct arena *a = context;

    a->used[physical / 4096] = 0;
}

static void *arena_map(void *context, uint64_t physical)
{
    struct arena *a = context;

    return a->memory + physical;
}

int main(void)
{
    struct arena arena;
    struct vmm_ops ops;
    struct vmm_space space;
    uint8_t buffer[8192];

    memset(&arena, 0, sizeof arena);
    arena.memory = calloc(ARENA_FRAMES, 4096);
    ops.alloc_frame = arena_alloc;
    ops.free_frame = arena_free;
    ops.map_physical = arena_map;
    ops.flush = NULL;
    ops.context = &arena;
    check("space", vmm_space_init(&space, &ops) == VMM_OK);

    for (unsigned int i = 0; i < 4096; i++) {
        arena.memory[DATA_FRAME_A + i] = (uint8_t)i;
        arena.memory[DATA_FRAME_B + i] = (uint8_t)(i + 4096);
    }

    check("map first user page", vmm_map(&space, 0x400000, DATA_FRAME_A, VMM_USER) == VMM_OK);
    check("map second user page", vmm_map(&space, 0x401000, DATA_FRAME_B, VMM_USER | VMM_WRITE) == VMM_OK);
    check("map a kernel only page", vmm_map(&space, 0x500000, DATA_FRAME_A, VMM_WRITE) == VMM_OK);

    memset(buffer, 0xEE, sizeof buffer);
    check("copy inside a page", uaccess_copy_from_user(&space, buffer, 0x400010, 16) == 0);
    check("copied bytes", buffer[0] == 0x10 && buffer[15] == 0x1F && buffer[16] == 0xEE);

    check("copy across a page boundary", uaccess_copy_from_user(&space, buffer, 0x400FF0, 32) == 0);
    int contiguous = 1;
    for (unsigned int i = 0; i < 32; i++) {
        if (buffer[i] != (uint8_t)(0xF0 + i))
            contiguous = 0;
    }
    check("bytes continue across the boundary", contiguous);

    check("copy a whole page", uaccess_copy_from_user(&space, buffer, 0x400000, 4096) == 0 && buffer[4095] == 0xFF);
    check("copy two whole pages", uaccess_copy_from_user(&space, buffer, 0x400000, 8192) == 0 && buffer[8191] == 0xFF &&
                                      buffer[4096] == 0x00);

    check("unmapped address", uaccess_copy_from_user(&space, buffer, 0x402000, 4) == -1);
    check("range reaching an unmapped page", uaccess_copy_from_user(&space, buffer, 0x401FF0, 32) == -1);
    check("kernel only page", uaccess_copy_from_user(&space, buffer, 0x500000, 4) == -1);
    check("address zero", uaccess_copy_from_user(&space, buffer, 0, 4) == -1);

    check("zero length is always fine", uaccess_copy_from_user(&space, buffer, 0xDEADBEEF, 0) == 0);
    check("kernel half address", uaccess_copy_from_user(&space, buffer, 0xFFFFFFFF80100000ull, 4) == -1);
    check("direct map address", uaccess_copy_from_user(&space, buffer, 0xFFFF800000000000ull, 4) == -1);
    check("first non user address", uaccess_copy_from_user(&space, buffer, UACCESS_USER_LIMIT, 1) == -1);
    check("range crossing the user limit", uaccess_copy_from_user(&space, buffer, UACCESS_USER_LIMIT - 4, 16) == -1);
    check("length that wraps around", uaccess_copy_from_user(&space, buffer, 0x400000, (size_t)-1) == -1);
    check("huge length", uaccess_copy_from_user(&space, buffer, 0x400000, (size_t)1 << 62) == -1);

    vmm_space_destroy(&space);
    free(arena.memory);
    if (failures == 0)
        printf("test_uaccess: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
