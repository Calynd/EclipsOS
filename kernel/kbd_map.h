#ifndef UDK_KBD_MAP_H
#define UDK_KBD_MAP_H

#include <stdint.h>

struct kbd_state {
    uint8_t shift;
    uint8_t caps;
    uint8_t extended;
};

char kbd_translate(struct kbd_state *state, uint8_t scancode);

#endif
