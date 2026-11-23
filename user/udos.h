#ifndef UDOS_USER_H
#define UDOS_USER_H

#include "udk_abi.h"

static inline long udos_syscall3(long number, long a, long b, long c)
{
    long result;

    __asm__ volatile("syscall" : "=a"(result) : "a"(number), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return result;
}

static inline long udos_debug_write(const char *text, unsigned long length)
{
    return udos_syscall3(UDK_SYS_DEBUG_WRITE, (long)text, (long)length, 0);
}

static inline __attribute__((noreturn)) void udos_exit(long code)
{
    udos_syscall3(UDK_SYS_EXIT, code, 0, 0);
    __builtin_unreachable();
}

static inline long udos_time(void)
{
    return udos_syscall3(UDK_SYS_TIME, 0, 0, 0);
}

#endif
