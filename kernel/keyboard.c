#include <stdint.h>
#include "charbuf.h"
#include "idt.h"
#include "interrupt.h"
#include "io.h"
#include "kbd_map.h"
#include "keyboard.h"
#include "pic.h"

#define KEYBOARD_DATA 0x60
#define KEYBOARD_STATUS 0x64
#define KEYBOARD_OUTPUT_FULL 0x01
#define KEYBOARD_IRQ 1

static struct kbd_state state;
static struct charbuf buffer;

static __attribute__((interrupt)) void keyboard_handler(struct interrupt_frame *frame)
{
    uint8_t scancode = inb(KEYBOARD_DATA);
    char c = kbd_translate(&state, scancode);

    (void)frame;
    if (c != 0)
        charbuf_push(&buffer, c);
    pic_eoi(KEYBOARD_IRQ);
}

void keyboard_init(void)
{
    while (inb(KEYBOARD_STATUS) & KEYBOARD_OUTPUT_FULL)
        inb(KEYBOARD_DATA);

    idt_set_handler(IRQ_VECTOR_BASE + KEYBOARD_IRQ, keyboard_handler);
    pic_unmask(KEYBOARD_IRQ);
}

int keyboard_read(char *c)
{
    int result;

    interrupts_disable();
    result = charbuf_pop(&buffer, c);
    interrupts_enable();
    return result;
}
