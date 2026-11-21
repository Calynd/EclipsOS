#include <stdint.h>
#include "gdt.h"
#include "idt.h"
#include "interrupt.h"

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_pointer idt_pointer;

static __attribute__((interrupt)) void ignore_interrupt(struct interrupt_frame *frame)
{
    (void)frame;
}

void idt_set_handler(uint8_t vector, void *handler)
{
    idt[vector] = idt_make_entry((uint64_t)(uintptr_t)handler, GDT_KERNEL_CODE, IDT_GATE_INTERRUPT);
}

void idt_init(void)
{
    for (unsigned int vector = 0; vector < IDT_ENTRIES; vector++)
        idt_set_handler((uint8_t)vector, ignore_interrupt);

    idt_pointer.limit = (uint16_t)(sizeof idt - 1);
    idt_pointer.base = (uint64_t)(uintptr_t)idt;
    __asm__ volatile("lidt %0" : : "m"(idt_pointer) : "memory");
}
