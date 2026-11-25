#ifndef ECLIPS_INTERRUPT_H
#define ECLIPS_INTERRUPT_H

#include <stdint.h>

struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

static inline void interrupts_enable(void)
{
    __asm__ volatile("sti" : : : "memory");
}

static inline void interrupts_disable(void)
{
    __asm__ volatile("cli" : : : "memory");
}

static inline void cpu_wait(void)
{
    __asm__ volatile("sti; hlt" : : : "memory");
}

#endif
