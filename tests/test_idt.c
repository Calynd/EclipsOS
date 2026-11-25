#include <stdio.h>
#include <string.h>
#include "idt.h"

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
    check("entry size", sizeof(struct idt_entry) == 16);

    uint64_t handler = 0x0000123456789ABCull;
    struct idt_entry e = idt_make_entry(handler, 0x08, IDT_GATE_INTERRUPT);
    uint64_t rebuilt = (uint64_t)e.offset_low | ((uint64_t)e.offset_mid << 16) | ((uint64_t)e.offset_high << 32);

    check("offset round trip", rebuilt == handler);
    check("selector", e.selector == 0x08);
    check("type", e.type_attr == 0x8E);
    check("ist", e.ist == 0);
    check("reserved", e.reserved == 0);

    uint8_t raw[16];
    memcpy(raw, &e, sizeof raw);
    check("byte 0", raw[0] == 0xBC && raw[1] == 0x9A);
    check("selector bytes", raw[2] == 0x08 && raw[3] == 0x00);
    check("ist and type bytes", raw[4] == 0x00 && raw[5] == 0x8E);
    check("mid bytes", raw[6] == 0x78 && raw[7] == 0x56);
    check("high bytes", raw[8] == 0x34 && raw[9] == 0x12 && raw[10] == 0x00 && raw[11] == 0x00);

    struct idt_entry kernel_high = idt_make_entry(0xFFFFFFFF80100000ull, 0x08, IDT_GATE_INTERRUPT);
    check("high half address", kernel_high.offset_high == 0xFFFFFFFFu && kernel_high.offset_mid == 0x8010 &&
                                   kernel_high.offset_low == 0x0000);

    if (failures == 0)
        printf("test_idt: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
