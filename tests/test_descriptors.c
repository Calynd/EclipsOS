#include <stddef.h>
#include <stdio.h>
#include "gdt.h"
#include "syscall.h"
#include "tss.h"
#include "uaccess.h"
#include "udk_abi.h"

static int failures;

static void check(const char *name, int condition)
{
    if (!condition) {
        printf("FAIL %s\n", name);
        failures++;
    }
}

int main(void)
{
    uint64_t descriptor[2];

    check("tss size", sizeof(struct tss) == 104);
    check("tss rsp0 offset", offsetof(struct tss, rsp0) == 4);
    check("tss ist offset", offsetof(struct tss, ist) == 36);
    check("tss io map offset", offsetof(struct tss, iomap_base) == 102);

    check("frame size", sizeof(struct syscall_frame) == 80);
    check("frame rax", offsetof(struct syscall_frame, rax) == 0);
    check("frame rdi", offsetof(struct syscall_frame, rdi) == 8);
    check("frame rsi", offsetof(struct syscall_frame, rsi) == 16);
    check("frame rdx", offsetof(struct syscall_frame, rdx) == 24);
    check("frame r10", offsetof(struct syscall_frame, r10) == 32);
    check("frame r8", offsetof(struct syscall_frame, r8) == 40);
    check("frame r9", offsetof(struct syscall_frame, r9) == 48);
    check("frame rip", offsetof(struct syscall_frame, rip) == 56);
    check("frame rflags", offsetof(struct syscall_frame, rflags) == 64);
    check("frame rsp", offsetof(struct syscall_frame, rsp) == 72);

    check("syscall stack segment follows the kernel code segment", GDT_KERNEL_DATA == GDT_KERNEL_CODE + 8);
    check("sysret user data layout", GDT_USER_DATA == ((GDT_SYSRET_BASE + 8) | 3));
    check("sysret user code layout", GDT_USER_CODE == ((GDT_SYSRET_BASE + 16) | 3));
    check("tss selector follows the user segments", GDT_TSS == (GDT_USER_CODE & ~3) + 8);
    check("tss selector alignment", (GDT_TSS % 8) == 0 && (GDT_TSS & 3) == 0);

    tss_make_descriptor(0xFFFFFFFF8012A5C0ull, 103, descriptor);
    check("tss descriptor low", descriptor[0] == 0x80008912A5C00067ull);
    check("tss descriptor high", descriptor[1] == 0x00000000FFFFFFFFull);

    tss_make_descriptor(0x0000000000001000ull, 0xFFFFF, descriptor);
    check("tss descriptor with a large limit", ((descriptor[0] >> 48) & 0xF) == 0xF && (descriptor[0] & 0xFFFF) == 0xFFFF);
    check("tss descriptor type", ((descriptor[0] >> 40) & 0xFF) == 0x89);

    check("user code base is page aligned", (UDK_USER_CODE_BASE & 0xFFF) == 0);
    check("user code base is in the user half", UDK_USER_CODE_BASE < UACCESS_USER_LIMIT);
    check("user stack top is page aligned", (UDK_USER_STACK_TOP & 0xFFF) == 0);
    check("user stack top is in the user half", UDK_USER_STACK_TOP < UACCESS_USER_LIMIT);
    check("user stack size is page aligned", (UDK_USER_STACK_SIZE & 0xFFF) == 0);
    check("user stack is above the code", UDK_USER_STACK_TOP - UDK_USER_STACK_SIZE > UDK_USER_CODE_BASE);

    check("syscall numbers are distinct", UDK_SYS_DEBUG_WRITE != UDK_SYS_EXIT && UDK_SYS_EXIT != UDK_SYS_TIME &&
                                              UDK_SYS_DEBUG_WRITE != UDK_SYS_TIME);
    check("error codes are negative", UDK_ERR_NO_SYSCALL < 0 && UDK_ERR_BAD_ADDRESS < 0 && UDK_ERR_INVALID < 0);

    if (failures == 0)
        printf("test_descriptors: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
