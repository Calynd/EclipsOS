#include <stdint.h>
#include "exception.h"
#include "gdt.h"
#include "idt.h"
#include "interrupt.h"
#include "keyboard.h"
#include "memory.h"
#include "pic.h"
#include "timer.h"
#include "udbl.h"
#include "vga.h"

#define TIMER_HZ 100u
#define INPUT_MAX 70u
#define CLOCK_ROW 0u
#define CLOCK_COL 68u

extern char __bss_start[];
extern char __bss_end[];

static __attribute__((noreturn)) void halt(void)
{
    for (;;)
        __asm__ volatile("cli; hlt");
}

static void clear_bss(void)
{
    for (volatile char *p = __bss_start; p < __bss_end; p++)
        *p = 0;
}

static uint64_t usable_memory(const struct udbl_info *info)
{
    uint64_t total = 0;

    for (uint32_t i = 0; i < info->mmap_count; i++) {
        if (info->mmap[i].type == UDBL_MMAP_USABLE)
            total += info->mmap[i].length;
    }
    return total;
}

static void format_uptime(char *out, uint64_t seconds)
{
    uint64_t hours = (seconds / 3600) % 100;
    uint64_t minutes = (seconds / 60) % 60;
    uint64_t secs = seconds % 60;

    out[0] = 'U';
    out[1] = 'P';
    out[2] = ' ';
    out[3] = (char)('0' + hours / 10);
    out[4] = (char)('0' + hours % 10);
    out[5] = ':';
    out[6] = (char)('0' + minutes / 10);
    out[7] = (char)('0' + minutes % 10);
    out[8] = ':';
    out[9] = (char)('0' + secs / 10);
    out[10] = (char)('0' + secs % 10);
    out[11] = 0;
}

static void echo_loop(void)
{
    char uptime[12];
    uint64_t last_second = (uint64_t)-1;
    unsigned int length = 0;

    vga_puts("> ");
    for (;;) {
        uint64_t second = timer_ticks() / timer_frequency();
        char c;

        if (second != last_second) {
            format_uptime(uptime, second);
            vga_write_at(CLOCK_ROW, CLOCK_COL, uptime);
            last_second = second;
        }

        if (!keyboard_read(&c)) {
            cpu_wait();
            continue;
        }

        if (c == '\n') {
            vga_puts("\n> ");
            length = 0;
        } else if (c == '\b') {
            if (length > 0) {
                length--;
                vga_putc('\b');
            }
        } else if (c >= 0x20 && c < 0x7F && length < INPUT_MAX) {
            length++;
            vga_putc(c);
        }
    }
}

__attribute__((noreturn, section(".text.entry")))
void udk_main(const struct udbl_info *info)
{
    clear_bss();

    if (info->magic != UDBL_MAGIC || info->version != UDBL_INFO_VERSION) {
        vga_init(0);
        vga_puts("UDK: INVALID BOOT INFORMATION\n");
        halt();
    }

    vga_init(info->cursor_row);
    vga_puts("UDK: ENTERED IN 64-BIT MODE\n");
    vga_puts("UDK: BOOT DRIVE ");
    vga_put_hex(info->boot_drive);
    vga_puts("\n");
    vga_puts("UDK: MEMORY MAP ENTRIES ");
    vga_put_dec(info->mmap_count);
    vga_puts("\n");
    vga_puts("UDK: USABLE MEMORY ");
    vga_put_dec(usable_memory(info) / 1024);
    vga_puts(" KIB\n");

    gdt_init();
    vga_puts("UDK: GDT LOADED\n");

    idt_init();
    exception_init();
    vga_puts("UDK: IDT LOADED, EXCEPTIONS 0-31 HANDLED\n");

    if (memory_init(info) != 0) {
        vga_puts("UDK: MEMORY INITIALIZATION FAILED\n");
        halt();
    }
    vga_puts("UDK: PHYSICAL MEMORY ");
    vga_put_dec(memory_free_frames());
    vga_puts(" FRAMES FREE (");
    vga_put_dec(memory_free_frames() * PMM_FRAME_SIZE / 1024);
    vga_puts(" KIB)\n");
    vga_puts("UDK: KERNEL HEAP ");
    vga_put_dec(memory_heap_size() / 1024);
    vga_puts(" KIB\n");

    int memory_result = memory_self_test();
    if (memory_result != 0) {
        vga_puts("UDK: MEMORY SELF-TEST FAILED, CODE ");
        vga_put_dec((uint64_t)memory_result);
        vga_puts("\n");
        halt();
    }
    vga_puts("UDK: MEMORY SELF-TEST OK\n");

    pic_init();
    vga_puts("UDK: PIC REMAPPED TO VECTORS 32-47\n");

    timer_init(TIMER_HZ);
    vga_puts("UDK: TIMER ");
    vga_put_dec(timer_frequency());
    vga_puts(" HZ\n");

    keyboard_init();
    vga_puts("UDK: KEYBOARD READY, US LAYOUT\n");

    interrupts_enable();
    vga_puts("UDK: INTERRUPTS ENABLED\n");

#ifdef UDK_FAULT_TEST
    vga_puts("UDK: FAULT TEST\n");
    __asm__ volatile("ud2");
#endif

    echo_loop();
    halt();
}
