#include <stddef.h>
#include <stdint.h>
#include "gdt.h"
#include "msr.h"
#include "syscall.h"
#include "timer.h"
#include "tss.h"
#include "uaccess.h"
#include "udk_abi.h"
#include "user.h"
#include "vga.h"
#include "vm.h"

#define RFLAGS_TF 0x100ull
#define RFLAGS_IF 0x200ull
#define RFLAGS_DF 0x400ull
#define RFLAGS_AC 0x40000ull

uint64_t syscall_user_rsp;
uint64_t syscall_kernel_rsp;

__asm__(
    ".text\n"
    ".globl syscall_entry\n"
    ".type syscall_entry, @function\n"
    "syscall_entry:\n"
    "    movq %rsp, syscall_user_rsp(%rip)\n"
    "    movq syscall_kernel_rsp(%rip), %rsp\n"
    "    pushq syscall_user_rsp(%rip)\n"
    "    pushq %r11\n"
    "    pushq %rcx\n"
    "    pushq %r9\n"
    "    pushq %r8\n"
    "    pushq %r10\n"
    "    pushq %rdx\n"
    "    pushq %rsi\n"
    "    pushq %rdi\n"
    "    pushq %rax\n"
    "    movq %rsp, %rdi\n"
    "    call syscall_dispatch\n"
    "    addq $8, %rsp\n"
    "    popq %rdi\n"
    "    popq %rsi\n"
    "    popq %rdx\n"
    "    popq %r10\n"
    "    popq %r8\n"
    "    popq %r9\n"
    "    popq %rcx\n"
    "    popq %r11\n"
    "    popq %rsp\n"
    "    sysretq\n"
    ".size syscall_entry, . - syscall_entry\n");

void syscall_init(void)
{
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_SCE);
    wrmsr(MSR_STAR, ((uint64_t)GDT_SYSRET_BASE << 48) | ((uint64_t)GDT_KERNEL_CODE << 32));
    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);
    wrmsr(MSR_SFMASK, RFLAGS_TF | RFLAGS_IF | RFLAGS_DF | RFLAGS_AC);
    syscall_kernel_rsp = tss_trap_stack_top();
}

static long debug_write(uint64_t user_pointer, uint64_t length)
{
    char buffer[128];
    uint64_t done = 0;

    if (length > UDK_DEBUG_WRITE_MAX)
        return UDK_ERR_INVALID;

    while (done < length) {
        size_t chunk = (size_t)(length - done);

        if (chunk > sizeof buffer)
            chunk = sizeof buffer;
        if (uaccess_copy_from_user(vm_kernel_space(), buffer, user_pointer + done, chunk) != 0)
            return UDK_ERR_BAD_ADDRESS;
        for (size_t i = 0; i < chunk; i++) {
            char c = buffer[i];

            if (c == '\n' || (c >= 0x20 && c < 0x7F))
                vga_putc(c);
        }
        done += chunk;
    }
    return (long)length;
}

long syscall_dispatch(struct syscall_frame *frame)
{
    switch (frame->rax) {
    case UDK_SYS_DEBUG_WRITE:
        return debug_write(frame->rdi, frame->rsi);
    case UDK_SYS_EXIT:
        user_exit((int64_t)frame->rdi);
    case UDK_SYS_TIME:
        return (long)timer_ticks();
    default:
        return UDK_ERR_NO_SYSCALL;
    }
}
