#ifndef UDK_PMM_H
#define UDK_PMM_H

#include <stddef.h>
#include <stdint.h>

#define PMM_FRAME_SIZE 4096u
#define PMM_NO_FRAME UINT64_MAX

struct pmm {
    uint8_t *bitmap;
    uint64_t frame_count;
    uint64_t free_count;
    uint64_t hint;
};

void pmm_init(struct pmm *pmm, uint8_t *bitmap, uint64_t frame_count);
void pmm_free_range(struct pmm *pmm, uint64_t start, uint64_t end);
void pmm_reserve_range(struct pmm *pmm, uint64_t start, uint64_t end);
uint64_t pmm_alloc(struct pmm *pmm);
uint64_t pmm_alloc_contiguous(struct pmm *pmm, uint64_t count);
int pmm_free(struct pmm *pmm, uint64_t address);
int pmm_is_free(const struct pmm *pmm, uint64_t address);

#endif
