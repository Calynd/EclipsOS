#include <stdio.h>
#include <string.h>
#include "kbd_map.h"

static int failures;

static void expect(const char *name, char got, char want)
{
    if (got != want) {
        printf("FAIL %s: got 0x%02x, want 0x%02x\n", name, (unsigned char)got, (unsigned char)want);
        failures++;
    }
}

static char press(struct kbd_state *state, uint8_t code)
{
    char c = kbd_translate(state, code);

    kbd_translate(state, (uint8_t)(code | 0x80));
    return c;
}

int main(void)
{
    struct kbd_state s;

    memset(&s, 0, sizeof s);
    expect("a", press(&s, 0x1E), 'a');
    expect("z", press(&s, 0x2C), 'z');
    expect("q", press(&s, 0x10), 'q');
    expect("m", press(&s, 0x32), 'm');
    expect("1", press(&s, 0x02), '1');
    expect("0", press(&s, 0x0B), '0');
    expect("minus", press(&s, 0x0C), '-');
    expect("equals", press(&s, 0x0D), '=');
    expect("space", press(&s, 0x39), ' ');
    expect("enter", press(&s, 0x1C), '\n');
    expect("backspace", press(&s, 0x0E), '\b');
    expect("tab", press(&s, 0x0F), '\t');
    expect("escape", press(&s, 0x01), 27);
    expect("semicolon", press(&s, 0x27), ';');
    expect("quote", press(&s, 0x28), '\'');
    expect("backtick", press(&s, 0x29), '`');
    expect("backslash", press(&s, 0x2B), '\\');
    expect("comma", press(&s, 0x33), ',');
    expect("period", press(&s, 0x34), '.');
    expect("slash", press(&s, 0x35), '/');
    expect("bracket left", press(&s, 0x1A), '[');
    expect("bracket right", press(&s, 0x1B), ']');
    expect("keypad star", press(&s, 0x37), '*');
    expect("ctrl", press(&s, 0x1D), 0);
    expect("alt", press(&s, 0x38), 0);
    expect("out of range", kbd_translate(&s, 0x3B), 0);
    expect("release gives nothing", kbd_translate(&s, 0x1E | 0x80), 0);

    kbd_translate(&s, 0x2A);
    expect("shift a", kbd_translate(&s, 0x1E), 'A');
    expect("shift 1", kbd_translate(&s, 0x02), '!');
    expect("shift minus", kbd_translate(&s, 0x0C), '_');
    expect("shift semicolon", kbd_translate(&s, 0x27), ':');
    expect("shift quote", kbd_translate(&s, 0x28), '"');
    expect("shift slash", kbd_translate(&s, 0x35), '?');
    expect("shift backtick", kbd_translate(&s, 0x29), '~');
    expect("shift backslash", kbd_translate(&s, 0x2B), '|');
    kbd_translate(&s, 0x2A | 0x80);
    expect("shift released", kbd_translate(&s, 0x1E), 'a');

    kbd_translate(&s, 0x36);
    expect("right shift b", kbd_translate(&s, 0x30), 'B');
    kbd_translate(&s, 0x36 | 0x80);

    expect("caps toggles silently", kbd_translate(&s, 0x3A), 0);
    kbd_translate(&s, 0x3A | 0x80);
    expect("caps a", kbd_translate(&s, 0x1E), 'A');
    expect("caps 1 stays digit", kbd_translate(&s, 0x02), '1');
    expect("caps semicolon stays", kbd_translate(&s, 0x27), ';');
    kbd_translate(&s, 0x2A);
    expect("caps+shift a", kbd_translate(&s, 0x1E), 'a');
    expect("caps+shift 1", kbd_translate(&s, 0x02), '!');
    kbd_translate(&s, 0x2A | 0x80);
    kbd_translate(&s, 0x3A);
    kbd_translate(&s, 0x3A | 0x80);
    expect("caps off", kbd_translate(&s, 0x1E), 'a');

    expect("extended prefix", kbd_translate(&s, 0xE0), 0);
    expect("extended key ignored", kbd_translate(&s, 0x1E), 0);
    expect("after extended", kbd_translate(&s, 0x1E), 'a');
    kbd_translate(&s, 0xE0);
    expect("extended release ignored", kbd_translate(&s, 0x1E | 0x80), 0);
    expect("after extended release", kbd_translate(&s, 0x1E), 'a');

    if (failures == 0)
        printf("test_kbd_map: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
