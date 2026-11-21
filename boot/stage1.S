.intel_syntax noprefix
.code16

#ifndef STAGE2_SECTORS
#define STAGE2_SECTORS 8
#endif

#define STAGE2_ADDR 0x7E00
#define STACK_TOP 0x7C00
#define READ_RETRIES 3

.section .text
.global _start

_start:
    cli
    push 0
    push offset stage1_main
    retf

stage1_main:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_TOP
    sti
    cld
    mov byte ptr [boot_drive], dl
    mov di, READ_RETRIES

read_stage2:
    mov ah, 0x02
    mov al, STAGE2_SECTORS
    xor ch, ch
    mov cl, 2
    xor dh, dh
    mov dl, byte ptr [boot_drive]
    mov bx, STAGE2_ADDR
    int 0x13
    jnc stage2_loaded
    xor ax, ax
    mov dl, byte ptr [boot_drive]
    int 0x13
    dec di
    jnz read_stage2
    mov si, offset msg_disk_error
    call print

halt:
    cli
    hlt
    jmp halt

stage2_loaded:
    mov dl, byte ptr [boot_drive]
    mov ax, STAGE2_ADDR
    jmp ax

print:
    lodsb
    test al, al
    jz print_done
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp print
print_done:
    ret

boot_drive:
    .byte 0

msg_disk_error:
    .asciz "UDBL: DISK READ ERROR"

.org 510
.word 0xAA55
