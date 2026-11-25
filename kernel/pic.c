#include <stdint.h>
#include "io.h"
#include "pic.h"

#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_COMMAND 0xA0
#define PIC_SLAVE_DATA 0xA1
#define PIC_EOI 0x20

void pic_init(void)
{
    outb(PIC_MASTER_COMMAND, 0x11);
    io_wait();
    outb(PIC_SLAVE_COMMAND, 0x11);
    io_wait();
    outb(PIC_MASTER_DATA, IRQ_VECTOR_BASE);
    io_wait();
    outb(PIC_SLAVE_DATA, IRQ_VECTOR_BASE + 8);
    io_wait();
    outb(PIC_MASTER_DATA, 0x04);
    io_wait();
    outb(PIC_SLAVE_DATA, 0x02);
    io_wait();
    outb(PIC_MASTER_DATA, 0x01);
    io_wait();
    outb(PIC_SLAVE_DATA, 0x01);
    io_wait();
    outb(PIC_MASTER_DATA, 0xFB);
    outb(PIC_SLAVE_DATA, 0xFF);
}

void pic_mask(uint8_t irq)
{
    uint16_t port = irq < 8 ? PIC_MASTER_DATA : PIC_SLAVE_DATA;
    uint8_t bit = (uint8_t)(1u << (irq & 7));

    outb(port, (uint8_t)(inb(port) | bit));
}

void pic_unmask(uint8_t irq)
{
    uint16_t port = irq < 8 ? PIC_MASTER_DATA : PIC_SLAVE_DATA;
    uint8_t bit = (uint8_t)(1u << (irq & 7));

    outb(port, (uint8_t)(inb(port) & ~bit));
}

void pic_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC_SLAVE_COMMAND, PIC_EOI);
    outb(PIC_MASTER_COMMAND, PIC_EOI);
}
