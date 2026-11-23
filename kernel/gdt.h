#ifndef UDK_GDT_H
#define UDK_GDT_H

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_SYSRET_BASE 0x18
#define GDT_USER_DATA 0x23
#define GDT_USER_CODE 0x2B
#define GDT_TSS 0x30

void gdt_init(void);

#endif
