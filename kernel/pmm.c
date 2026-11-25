#include "pmm.h"

static int bit_get(const struct pmm *pmm, uint64_t frame)
{
    return (pmm->bitmap[frame / 8] >> (frame % 8)) & 1;
}

static void bit_set(struct pmm *pmm, uint64_t frame)
{
    pmm->bitmap[frame / 8] = (uint8_t)(pmm->bitmap[frame / 8] | (1u << (frame % 8)));
}

static void bit_clear(struct pmm *pmm, uint64_t frame)
{
    pmm->bitmap[frame / 8] = (uint8_t)(pmm->bitmap[frame / 8] & ~(1u << (frame % 8)));
}

void pmm_init(struct pmm *pmm, uint8_t *bitmap, uint64_t frame_count)
{
    uint64_t bytes = (frame_count + 7) / 8;

    pmm->bitmap = bitmap;
    pmm->frame_count = frame_count;
    pmm->free_count = 0;
    pmm->hint = 0;
    for (uint64_t i = 0; i < bytes; i++)
        bitmap[i] = 0xFF;
}

void pmm_free_range(struct pmm *pmm, uint64_t start, uint64_t end)
{
    uint64_t first = (start + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    uint64_t last = end / PMM_FRAME_SIZE;

    if (start > UINT64_MAX - PMM_FRAME_SIZE)
        return;
    if (last > pmm->frame_count)
        last = pmm->frame_count;
    for (uint64_t frame = first; frame < last; frame++) {
        if (bit_get(pmm, frame)) {
            bit_clear(pmm, frame);
            pmm->free_count++;
        }
    }
}

void pmm_reserve_range(struct pmm *pmm, uint64_t start, uint64_t end)
{
    uint64_t first = start / PMM_FRAME_SIZE;
    uint64_t last = end > UINT64_MAX - PMM_FRAME_SIZE ? pmm->frame_count
                                                      : (end + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;

    if (last > pmm->frame_count)
        last = pmm->frame_count;
    for (uint64_t frame = first; frame < last; frame++) {
        if (!bit_get(pmm, frame)) {
            bit_set(pmm, frame);
            pmm->free_count--;
        }
    }
}

uint64_t pmm_alloc(struct pmm *pmm)
{
    if (pmm->free_count == 0 || pmm->frame_count == 0)
        return PMM_NO_FRAME;

    for (uint64_t step = 0; step < pmm->frame_count; step++) {
        uint64_t frame = (pmm->hint + step) % pmm->frame_count;

        if (!bit_get(pmm, frame)) {
            bit_set(pmm, frame);
            pmm->free_count--;
            pmm->hint = frame + 1;
            return frame * PMM_FRAME_SIZE;
        }
    }
    return PMM_NO_FRAME;
}

uint64_t pmm_alloc_contiguous(struct pmm *pmm, uint64_t count)
{
    uint64_t run = 0;

    if (count == 0 || count > pmm->free_count)
        return PMM_NO_FRAME;

    for (uint64_t frame = 0; frame < pmm->frame_count; frame++) {
        if (bit_get(pmm, frame)) {
            run = 0;
            continue;
        }
        run++;
        if (run == count) {
            uint64_t first = frame + 1 - count;

            for (uint64_t i = first; i <= frame; i++)
                bit_set(pmm, i);
            pmm->free_count -= count;
            return first * PMM_FRAME_SIZE;
        }
    }
    return PMM_NO_FRAME;
}

int pmm_free(struct pmm *pmm, uint64_t address)
{
    uint64_t frame;

    if (address % PMM_FRAME_SIZE != 0)
        return -1;
    frame = address / PMM_FRAME_SIZE;
    if (frame >= pmm->frame_count)
        return -1;
    if (!bit_get(pmm, frame))
        return -2;
    bit_clear(pmm, frame);
    pmm->free_count++;
    return 0;
}

int pmm_is_free(const struct pmm *pmm, uint64_t address)
{
    uint64_t frame = address / PMM_FRAME_SIZE;

    if (frame >= pmm->frame_count)
        return 0;
    return !bit_get(pmm, frame);
}
