#ifndef ECLIPS_SYSCALL_H
#define ECLIPS_SYSCALL_H

#include <stdint.h>

struct syscall_frame {
    uint64_t rax;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t r10;
    uint64_t r8;
    uint64_t r9;
    uint64_t rip;
    uint64_t rflags;
    uint64_t rsp;
};

_Static_assert(sizeof(struct syscall_frame) == 80, "syscall frame size");

void syscall_entry(void);
void syscall_init(void);
long syscall_dispatch(struct syscall_frame *frame);

#endif
