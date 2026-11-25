#include <stdint.h>
#include "exception.h"
#include "idt.h"
#include "interrupt.h"
#include "tss.h"
#include "vga.h"

static const char *const exception_names[32] = {
    "DIVIDE ERROR",
    "DEBUG",
    "NON-MASKABLE INTERRUPT",
    "BREAKPOINT",
    "OVERFLOW",
    "BOUND RANGE EXCEEDED",
    "INVALID OPCODE",
    "DEVICE NOT AVAILABLE",
    "DOUBLE FAULT",
    "COPROCESSOR SEGMENT OVERRUN",
    "INVALID TSS",
    "SEGMENT NOT PRESENT",
    "STACK-SEGMENT FAULT",
    "GENERAL PROTECTION FAULT",
    "PAGE FAULT",
    "RESERVED",
    "X87 FLOATING-POINT ERROR",
    "ALIGNMENT CHECK",
    "MACHINE CHECK",
    "SIMD FLOATING-POINT ERROR",
    "VIRTUALIZATION EXCEPTION",
    "CONTROL PROTECTION",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "RESERVED",
    "HYPERVISOR INJECTION",
    "VMM COMMUNICATION",
    "SECURITY EXCEPTION",
    "RESERVED",
};

static void report_line(const char *label, uint64_t value)
{
    vga_puts(label);
    vga_put_hex(value);
    vga_puts("\n");
}

static __attribute__((noreturn)) void exception_report(unsigned int vector, int has_error, uint64_t error_code,
                                                       const struct interrupt_frame *frame)
{
    vga_puts("\nKERNEL: EXCEPTION ");
    vga_put_dec(vector);
    vga_puts(" ");
    vga_puts(exception_names[vector]);
    vga_puts("\n");
    if ((frame->cs & 3) == 3)
        vga_puts("KERNEL: FAULT IN USER MODE\n");
    if (has_error)
        report_line("KERNEL: ERROR CODE ", error_code);
    report_line("KERNEL: RIP ", frame->rip);
    report_line("KERNEL: CS ", frame->cs);
    report_line("KERNEL: RFLAGS ", frame->rflags);
    report_line("KERNEL: RSP ", frame->rsp);
    if (vector == 14) {
        uint64_t address;

        __asm__ volatile("movq %%cr2, %0" : "=r"(address));
        report_line("KERNEL: FAULT ADDRESS ", address);
        vga_puts("KERNEL: ");
        vga_puts((error_code & 1) ? "PROTECTION VIOLATION" : "PAGE NOT PRESENT");
        vga_puts((error_code & 2) ? ", WRITE" : ", READ");
        vga_puts((error_code & 4) ? ", USER" : ", SUPERVISOR");
        if (error_code & 8)
            vga_puts(", RESERVED BIT SET");
        if (error_code & 16)
            vga_puts(", INSTRUCTION FETCH");
        vga_puts("\n");
    }
    vga_puts("KERNEL: SYSTEM HALTED\n");
    for (;;)
        __asm__ volatile("cli; hlt");
}

#define DEFINE_EXCEPTION(n)                                                         \
    static __attribute__((interrupt)) void exception_##n(struct interrupt_frame *f) \
    {                                                                               \
        exception_report(n, 0, 0, f);                                               \
    }

#define DEFINE_EXCEPTION_WITH_ERROR(n)                                                          \
    static __attribute__((interrupt)) void exception_##n(struct interrupt_frame *f, uint64_t c) \
    {                                                                                           \
        exception_report(n, 1, c, f);                                                           \
    }

DEFINE_EXCEPTION(0)
DEFINE_EXCEPTION(1)
DEFINE_EXCEPTION(2)
DEFINE_EXCEPTION(3)
DEFINE_EXCEPTION(4)
DEFINE_EXCEPTION(5)
DEFINE_EXCEPTION(6)
DEFINE_EXCEPTION(7)
DEFINE_EXCEPTION_WITH_ERROR(8)
DEFINE_EXCEPTION(9)
DEFINE_EXCEPTION_WITH_ERROR(10)
DEFINE_EXCEPTION_WITH_ERROR(11)
DEFINE_EXCEPTION_WITH_ERROR(12)
DEFINE_EXCEPTION_WITH_ERROR(13)
DEFINE_EXCEPTION_WITH_ERROR(14)
DEFINE_EXCEPTION(15)
DEFINE_EXCEPTION(16)
DEFINE_EXCEPTION_WITH_ERROR(17)
DEFINE_EXCEPTION(18)
DEFINE_EXCEPTION(19)
DEFINE_EXCEPTION(20)
DEFINE_EXCEPTION_WITH_ERROR(21)
DEFINE_EXCEPTION(22)
DEFINE_EXCEPTION(23)
DEFINE_EXCEPTION(24)
DEFINE_EXCEPTION(25)
DEFINE_EXCEPTION(26)
DEFINE_EXCEPTION(27)
DEFINE_EXCEPTION(28)
DEFINE_EXCEPTION_WITH_ERROR(29)
DEFINE_EXCEPTION_WITH_ERROR(30)
DEFINE_EXCEPTION(31)

void exception_init(void)
{
    void *handlers[32] = {
        exception_0,  exception_1,  exception_2,  exception_3,  exception_4,  exception_5,  exception_6,
        exception_7,  exception_8,  exception_9,  exception_10, exception_11, exception_12, exception_13,
        exception_14, exception_15, exception_16, exception_17, exception_18, exception_19, exception_20,
        exception_21, exception_22, exception_23, exception_24, exception_25, exception_26, exception_27,
        exception_28, exception_29, exception_30, exception_31,
    };

    for (unsigned int vector = 0; vector < 32; vector++)
        idt_set_handler((uint8_t)vector, handlers[vector]);
    idt_set_handler_ist(8, handlers[8], TSS_DOUBLE_FAULT_IST);
}
