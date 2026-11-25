#include <stdint.h>
#include "tss.h"

static struct tss tss;
static uint8_t trap_stack[TSS_TRAP_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t double_fault_stack[TSS_DOUBLE_FAULT_STACK_SIZE] __attribute__((aligned(16)));

uint64_t tss_trap_stack_top(void)
{
    return (uint64_t)(uintptr_t)(trap_stack + sizeof trap_stack);
}

void tss_init(void)
{
    tss.iomap_base = (uint16_t)sizeof tss;
    tss.rsp0 = tss_trap_stack_top();
    tss.ist[TSS_DOUBLE_FAULT_IST - 1] = (uint64_t)(uintptr_t)(double_fault_stack + sizeof double_fault_stack);
}

const struct tss *tss_get(void)
{
    return &tss;
}
