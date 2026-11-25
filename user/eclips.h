#ifndef ECLIPS_USER_LIB_H
#define ECLIPS_USER_LIB_H

#include "eclips_abi.h"

static inline long eclips_syscall3(long number, long a, long b, long c)
{
    long result;

    __asm__ volatile("syscall" : "=a"(result) : "a"(number), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return result;
}

static inline long eclips_debug_write(const char *text, unsigned long length)
{
    return eclips_syscall3(ECLIPS_SYS_DEBUG_WRITE, (long)text, (long)length, 0);
}

static inline __attribute__((noreturn)) void eclips_exit(long code)
{
    eclips_syscall3(ECLIPS_SYS_EXIT, code, 0, 0);
    __builtin_unreachable();
}

static inline long eclips_time(void)
{
    return eclips_syscall3(ECLIPS_SYS_TIME, 0, 0, 0);
}

#endif
