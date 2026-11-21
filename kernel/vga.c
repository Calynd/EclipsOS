#include "io.h"
#include "vga.h"

#define VGA_BASE ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80u
#define VGA_ROWS 25u
#define VGA_ATTR 0x07u
#define VGA_BLANK ((uint16_t)((VGA_ATTR << 8) | ' '))
#define VGA_ATTR_STATUS 0x70u

static unsigned int cursor_row;
static unsigned int cursor_col;

static void update_cursor(void)
{
    uint16_t pos = (uint16_t)(cursor_row * VGA_COLS + cursor_col);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)(pos >> 8));
}

static void scroll(void)
{
    for (unsigned int i = 0; i < (VGA_ROWS - 1) * VGA_COLS; i++)
        VGA_BASE[i] = VGA_BASE[i + VGA_COLS];
    for (unsigned int i = (VGA_ROWS - 1) * VGA_COLS; i < VGA_ROWS * VGA_COLS; i++)
        VGA_BASE[i] = VGA_BLANK;
    cursor_row = VGA_ROWS - 1;
}

static void newline(void)
{
    cursor_col = 0;
    cursor_row++;
    if (cursor_row >= VGA_ROWS)
        scroll();
}

void vga_init(unsigned int row)
{
    cursor_row = row < VGA_ROWS ? row : VGA_ROWS - 1;
    cursor_col = 0;
    update_cursor();
}

void vga_putc(char c)
{
    if (c == '\n') {
        newline();
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA_BASE[cursor_row * VGA_COLS + cursor_col] = VGA_BLANK;
        }
    } else {
        VGA_BASE[cursor_row * VGA_COLS + cursor_col] = (uint16_t)((VGA_ATTR << 8) | (uint8_t)c);
        cursor_col++;
        if (cursor_col >= VGA_COLS)
            newline();
    }
    update_cursor();
}

void vga_puts(const char *s)
{
    while (*s)
        vga_putc(*s++);
}

void vga_put_dec(uint64_t value)
{
    char digits[20];
    unsigned int count = 0;

    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);

    while (count > 0)
        vga_putc(digits[--count]);
}

void vga_put_hex(uint64_t value)
{
    static const char table[] = "0123456789ABCDEF";
    char digits[16];
    unsigned int count = 0;

    do {
        digits[count++] = table[value & 0xF];
        value >>= 4;
    } while (value != 0);

    vga_puts("0x");
    while (count > 0)
        vga_putc(digits[--count]);
}

void vga_write_at(unsigned int row, unsigned int col, const char *s)
{
    while (*s && row < VGA_ROWS && col < VGA_COLS) {
        VGA_BASE[row * VGA_COLS + col] = (uint16_t)((VGA_ATTR_STATUS << 8) | (uint8_t)*s);
        s++;
        col++;
    }
}
