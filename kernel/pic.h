#ifndef UDK_PIC_H
#define UDK_PIC_H

#include <stdint.h>

#define IRQ_VECTOR_BASE 32

void pic_init(void);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);
void pic_eoi(uint8_t irq);

#endif
