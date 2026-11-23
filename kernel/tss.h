#ifndef UDK_TSS_H
#define UDK_TSS_H

#include <stdint.h>

#define TSS_TRAP_STACK_SIZE 16384u
#define TSS_DOUBLE_FAULT_STACK_SIZE 8192u
#define TSS_DOUBLE_FAULT_IST 1u

struct tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

_Static_assert(sizeof(struct tss) == 104, "TSS size");

static inline void tss_make_descriptor(uint64_t base, uint32_t limit, uint64_t out[2])
{
    out[0] = (uint64_t)(limit & 0xFFFFu) | ((base & 0xFFFFFFull) << 16) | (0x89ull << 40) |
             ((uint64_t)((limit >> 16) & 0xFu) << 48) | (((base >> 24) & 0xFFull) << 56);
    out[1] = base >> 32;
}

void tss_init(void);
const struct tss *tss_get(void);
uint64_t tss_trap_stack_top(void);

#endif
