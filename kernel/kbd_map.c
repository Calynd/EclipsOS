#include "kbd_map.h"

static const char normal_map[] = "\0\033" "1234567890-=\b\t" "qwertyuiop[]\n\0" "asdfghjkl;'`\0\\" "zxcvbnm,./\0*\0 ";
static const char shifted_map[] = "\0\033" "!@#$%^&*()_+\b\t" "QWERTYUIOP{}\n\0" "ASDFGHJKL:\"~\0|" "ZXCVBNM<>?\0*\0 ";

#define MAP_LENGTH (sizeof normal_map - 1)
#define SCANCODE_EXTENDED 0xE0
#define SCANCODE_LEFT_SHIFT 0x2A
#define SCANCODE_RIGHT_SHIFT 0x36
#define SCANCODE_CAPS_LOCK 0x3A

char kbd_translate(struct kbd_state *state, uint8_t scancode)
{
    uint8_t released = scancode & 0x80;
    uint8_t code = scancode & 0x7F;
    int use_shift;
    char base;

    if (scancode == SCANCODE_EXTENDED) {
        state->extended = 1;
        return 0;
    }
    if (state->extended) {
        state->extended = 0;
        return 0;
    }
    if (code == SCANCODE_LEFT_SHIFT || code == SCANCODE_RIGHT_SHIFT) {
        state->shift = released ? 0 : 1;
        return 0;
    }
    if (released)
        return 0;
    if (code == SCANCODE_CAPS_LOCK) {
        state->caps ^= 1;
        return 0;
    }
    if (code >= MAP_LENGTH)
        return 0;

    base = normal_map[code];
    use_shift = state->shift;
    if (base >= 'a' && base <= 'z' && state->caps)
        use_shift = !use_shift;
    return use_shift ? shifted_map[code] : base;
}
