#ifndef ECLIPS_VGA_H
#define ECLIPS_VGA_H

#include <stdint.h>

void vga_init(unsigned int row);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_put_dec(uint64_t value);
void vga_put_hex(uint64_t value);
void vga_write_at(unsigned int row, unsigned int col, const char *s);

#endif
