#include <stddef.h>
#include <stdint.h>
#include "gdt.h"
#include "memory.h"
#include "udk_abi.h"
#include "user.h"
#include "vga.h"
#include "vm.h"
#include "vmm.h"

#define USER_CODE_PAGES_MAX 16u
#define USER_RFLAGS 0x202ull

extern const uint8_t user_hello_start[];
extern const uint8_t user_hello_end[];

static void (*resume_function)(void);
static unsigned int mapped_code_pages;

static void release_page(uint64_t address)
{
    uint64_t physical;

    if (vmm_unmap(vm_kernel_space(), address, &physical) == VMM_OK)
        memory_free_frame(physical);
}

static void release_user_memory(void)
{
    for (unsigned int i = 0; i < mapped_code_pages; i++)
        release_page(UDK_USER_CODE_BASE + (uint64_t)i * VMM_PAGE_SIZE);
    for (uint64_t address = UDK_USER_STACK_TOP - UDK_USER_STACK_SIZE; address < UDK_USER_STACK_TOP;
         address += VMM_PAGE_SIZE)
        release_page(address);
    mapped_code_pages = 0;
}

static int map_fresh_page(uint64_t address, uint64_t flags, const uint8_t *source, size_t length)
{
    uint64_t frame = memory_alloc_frame();
    uint8_t *page;

    if (frame == PMM_NO_FRAME)
        return -1;
    page = phys_to_virt(frame);
    for (size_t i = 0; i < VMM_PAGE_SIZE; i++)
        page[i] = i < length ? source[i] : 0;
    if (vmm_map(vm_kernel_space(), address, frame, flags) != VMM_OK) {
        memory_free_frame(frame);
        return -1;
    }
    return 0;
}

int user_run(void (*after_exit)(void))
{
    size_t size = (size_t)(user_hello_end - user_hello_start);
    unsigned int pages = (unsigned int)((size + VMM_PAGE_SIZE - 1) / VMM_PAGE_SIZE);
    uint64_t stack_top = UDK_USER_STACK_TOP;
    uint64_t entry = UDK_USER_CODE_BASE;

    if (pages == 0 || pages > USER_CODE_PAGES_MAX)
        return -1;
    resume_function = after_exit;

    for (unsigned int i = 0; i < pages; i++) {
        size_t offset = (size_t)i * VMM_PAGE_SIZE;

        if (map_fresh_page(UDK_USER_CODE_BASE + offset, VMM_USER, user_hello_start + offset, size - offset) != 0) {
            release_user_memory();
            return -1;
        }
        mapped_code_pages++;
    }
    for (uint64_t address = UDK_USER_STACK_TOP - UDK_USER_STACK_SIZE; address < UDK_USER_STACK_TOP;
         address += VMM_PAGE_SIZE) {
        if (map_fresh_page(address, VMM_USER | VMM_WRITE, NULL, 0) != 0) {
            release_user_memory();
            return -1;
        }
    }

    __asm__ volatile(
        "pushq %[ss]\n\t"
        "pushq %[sp]\n\t"
        "pushq %[fl]\n\t"
        "pushq %[cs]\n\t"
        "pushq %[ip]\n\t"
        "iretq"
        :
        : [ss] "i"(GDT_USER_DATA), [sp] "r"(stack_top), [fl] "i"(USER_RFLAGS), [cs] "i"(GDT_USER_CODE),
          [ip] "r"(entry)
        : "memory");
    __builtin_unreachable();
}

void user_exit(int64_t code)
{
    vga_puts("UDK: USER PROGRAM EXITED WITH CODE ");
    if (code < 0) {
        vga_putc('-');
        vga_put_dec((uint64_t)(-code));
    } else {
        vga_put_dec((uint64_t)code);
    }
    vga_puts("\n");

    release_user_memory();
    vga_puts("UDK: USER MEMORY RELEASED, ");
    vga_put_dec(memory_free_frames());
    vga_puts(" FRAMES FREE\n");

    resume_function();
    for (;;)
        __asm__ volatile("cli; hlt");
}
