#include <stdint.h>
#include "idt.h"
#include "interrupt.h"
#include "io.h"
#include "pic.h"
#include "timer.h"

#define PIT_FREQUENCY 1193182u
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43
#define PIT_SQUARE_WAVE 0x36
#define TIMER_IRQ 0

static volatile uint64_t ticks;
static uint32_t frequency;

static __attribute__((interrupt)) void timer_handler(struct interrupt_frame *frame)
{
    (void)frame;
    ticks++;
    pic_eoi(TIMER_IRQ);
}

void timer_init(uint32_t hz)
{
    uint32_t divisor = PIT_FREQUENCY / hz;

    if (divisor == 0 || divisor > 0xFFFF)
        divisor = 0xFFFF;
    frequency = PIT_FREQUENCY / divisor;

    outb(PIT_COMMAND, PIT_SQUARE_WAVE);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)(divisor >> 8));

    idt_set_handler(IRQ_VECTOR_BASE + TIMER_IRQ, timer_handler);
    pic_unmask(TIMER_IRQ);
}

uint64_t timer_ticks(void)
{
    return ticks;
}

uint32_t timer_frequency(void)
{
    return frequency;
}
