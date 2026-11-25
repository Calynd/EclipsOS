#include <stdio.h>
#include <string.h>
#include "pmm.h"

#define FRAMES 64

static int failures;

static void check(const char *name, int condition)
{
    if (!condition) {
        printf("FAIL %s\n", name);
        failures++;
    }
}

static void fresh(struct pmm *pmm, uint8_t *bitmap)
{
    memset(bitmap, 0, FRAMES / 8);
    pmm_init(pmm, bitmap, FRAMES);
}

int main(void)
{
    struct pmm pmm;
    uint8_t bitmap[FRAMES / 8];

    fresh(&pmm, bitmap);
    check("all used after init", pmm.free_count == 0);
    check("alloc on empty fails", pmm_alloc(&pmm) == PMM_NO_FRAME);
    check("is_free false", !pmm_is_free(&pmm, 0x2000));

    pmm_free_range(&pmm, 0x2000, 0x10000);
    check("range frees 14 frames", pmm.free_count == 14);
    check("frame 2 free", pmm_is_free(&pmm, 0x2000));
    check("frame 1 used", !pmm_is_free(&pmm, 0x1000));
    check("frame 15 free", pmm_is_free(&pmm, 0xF000));
    check("frame 16 used", !pmm_is_free(&pmm, 0x10000));

    pmm_free_range(&pmm, 0x2000, 0x10000);
    check("freeing twice does not miscount", pmm.free_count == 14);

    fresh(&pmm, bitmap);
    pmm_free_range(&pmm, 0x1001, 0x3FFF);
    check("free range rounds inward", pmm.free_count == 1 && pmm_is_free(&pmm, 0x2000));

    fresh(&pmm, bitmap);
    pmm_free_range(&pmm, 0, FRAMES * PMM_FRAME_SIZE);
    check("whole range", pmm.free_count == FRAMES);
    pmm_reserve_range(&pmm, 0x2800, 0x3800);
    check("reserve rounds outward", pmm.free_count == FRAMES - 2);
    check("reserved frame 2", !pmm_is_free(&pmm, 0x2000));
    check("reserved frame 3", !pmm_is_free(&pmm, 0x3000));
    check("frame 4 untouched", pmm_is_free(&pmm, 0x4000));
    pmm_reserve_range(&pmm, 0x2800, 0x3800);
    check("reserving twice does not miscount", pmm.free_count == FRAMES - 2);

    fresh(&pmm, bitmap);
    pmm_free_range(&pmm, 0, 0x100000000ull);
    check("range beyond the end is clipped", pmm.free_count == FRAMES);
    pmm_reserve_range(&pmm, 0, UINT64_MAX);
    check("reserve to the end of the address space", pmm.free_count == 0);

    fresh(&pmm, bitmap);
    pmm_free_range(&pmm, 0x4000, 0x4000 + 10 * PMM_FRAME_SIZE);
    uint64_t seen[10];
    int distinct = 1;
    for (int i = 0; i < 10; i++) {
        seen[i] = pmm_alloc(&pmm);
        check("alloc in range", seen[i] >= 0x4000 && seen[i] < 0x4000 + 10 * PMM_FRAME_SIZE);
        check("alloc aligned", seen[i] % PMM_FRAME_SIZE == 0);
        for (int j = 0; j < i; j++) {
            if (seen[i] == seen[j])
                distinct = 0;
        }
    }
    check("allocations are distinct", distinct);
    check("exhausted", pmm_alloc(&pmm) == PMM_NO_FRAME && pmm.free_count == 0);

    check("free works", pmm_free(&pmm, seen[3]) == 0);
    check("double free detected", pmm_free(&pmm, seen[3]) == -2);
    check("unaligned free rejected", pmm_free(&pmm, seen[4] + 1) == -1);
    check("out of range free rejected", pmm_free(&pmm, FRAMES * PMM_FRAME_SIZE) == -1);
    check("freed frame is reused", pmm_alloc(&pmm) == seen[3]);

    fresh(&pmm, bitmap);
    pmm_free_range(&pmm, 0x2000, 0x5000);
    pmm_free_range(&pmm, 0x8000, 0xE000);
    check("contiguous count 4 skips the short run", pmm_alloc_contiguous(&pmm, 4) == 0x8000);
    check("free count after contiguous", pmm.free_count == 5);
    check("contiguous count 3 uses the first run", pmm_alloc_contiguous(&pmm, 3) == 0x2000);
    check("contiguous too long fails", pmm_alloc_contiguous(&pmm, 3) == PMM_NO_FRAME);
    check("contiguous zero fails", pmm_alloc_contiguous(&pmm, 0) == PMM_NO_FRAME);
    check("contiguous more than free fails", pmm_alloc_contiguous(&pmm, 100) == PMM_NO_FRAME);
    check("remaining frames are free", pmm.free_count == 2 && pmm_is_free(&pmm, 0xC000) && pmm_is_free(&pmm, 0xD000));

    fresh(&pmm, bitmap);
    pmm_free_range(&pmm, 0, FRAMES * PMM_FRAME_SIZE);
    for (int i = 0; i < FRAMES; i++)
        check("sequential alloc", pmm_alloc(&pmm) != PMM_NO_FRAME);
    check("full", pmm.free_count == 0 && pmm_alloc(&pmm) == PMM_NO_FRAME);

    if (failures == 0)
        printf("test_pmm: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
