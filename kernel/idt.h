#ifndef UDK_IDT_H
#define UDK_IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256
#define IDT_GATE_INTERRUPT 0x8E

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

static inline struct idt_entry idt_make_entry(uint64_t handler, uint16_t selector, uint8_t type_attr)
{
    struct idt_entry entry;

    entry.offset_low = (uint16_t)(handler & 0xFFFF);
    entry.selector = selector;
    entry.ist = 0;
    entry.type_attr = type_attr;
    entry.offset_mid = (uint16_t)((handler >> 16) & 0xFFFF);
    entry.offset_high = (uint32_t)(handler >> 32);
    entry.reserved = 0;
    return entry;
}

void idt_init(void);
void idt_set_handler(uint8_t vector, void *handler);
void idt_set_handler_ist(uint8_t vector, void *handler, uint8_t ist);

#endif
