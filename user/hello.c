#include "eclips.h"

static unsigned long string_length(const char *text)
{
    unsigned long length = 0;

    while (text[length] != 0)
        length++;
    return length;
}

static void write_string(const char *text)
{
    eclips_debug_write(text, string_length(text));
}

static void write_decimal(unsigned long value)
{
    char digits[20];
    char text[20];
    unsigned int count = 0;
    unsigned int length = 0;

    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);

    while (count > 0)
        text[length++] = digits[--count];
    eclips_debug_write(text, length);
}

__attribute__((noreturn, section(".text.start")))
void _start(void)
{
#ifdef USER_TEST
#if USER_TEST == 1
    *(volatile unsigned long *)0 = 1;
#elif USER_TEST == 2
    (void)*(volatile unsigned long *)0xFFFFFFFF80100000ull;
#elif USER_TEST == 3
    __asm__ volatile("hlt");
#endif
#endif

    write_string("USER: HELLO FROM USER MODE\n");
    write_string("USER: UPTIME TICKS ");
    write_decimal((unsigned long)eclips_time());
    write_string("\n");
    eclips_exit(0);
}
