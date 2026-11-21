.intel_syntax noprefix
.code16

#define STACK_TOP 0x7C00
#define LINE_MAX 63
#define READ_RETRIES 3

#ifndef BOOT_DOTS
#define BOOT_DOTS 18
#endif

#define INFO_BASE 0x1000
#define INFO_MMAP_COUNT 0x1008
#define INFO_MMAP_ENTRIES 0x1010
#define MMAP_MAX 64

#define SB_BUF 0x5000
#define BLK_BUF 0x5200
#define DIR_BUF 0x5400
#define INODE_BUF 0x5600

#define VOL_START_LBA 16
#define UDFS_MAGIC_VALUE 0x53464455
#define KERNEL_MAX_BYTES 0x70000

#define KERNEL_TEMP_SEG 0x1000
#define KERNEL_TEMP_ADDR 0x10000
#define KERNEL_DEST 0x100000

#define PML4_ADDR 0x2000
#define PDPT_ADDR 0x3000
#define PD_ADDR 0x4000

#define STACK32_TOP 0x90000
#define STACK64_TOP 0x8FFF8

#define ERR_DISK 1
#define ERR_FS 2
#define ERR_NOT_FOUND 3
#define ERR_INVALID 4

.section .text
.global _start

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_TOP
    sti
    cld
    mov byte ptr [boot_drive], dl
    mov ax, 0x0003
    int 0x10

prompt_loop:
    mov si, offset msg_prompt
    call print
    call read_line
    mov si, offset line_buffer
    cmp byte ptr [si], 0
    je prompt_loop
    mov di, offset cmd_start
    call match_word
    jz command_start
    mov si, offset msg_unknown
    call print
    jmp prompt_loop

command_start:
    mov word ptr [flags_ptr], si
    mov si, offset msg_loading
    call print
    mov word ptr [dots_printed], 0
    mov eax, dword ptr [0x046C]
    mov dword ptr [dot_start], eax
    call load_kernel
    jnc kernel_loaded
    mov si, offset msg_failed
    call print
    mov al, byte ptr [load_error]
    mov si, offset msg_err_disk
    cmp al, ERR_DISK
    je load_error_print
    mov si, offset msg_err_fs
    cmp al, ERR_FS
    je load_error_print
    mov si, offset msg_err_not_found
    cmp al, ERR_NOT_FOUND
    je load_error_print
    mov si, offset msg_err_invalid

load_error_print:
    call print
    jmp prompt_loop

kernel_loaded:
    call wait_dots
    mov si, offset msg_ok
    call print
    call detect_memory
    jnc memory_ready
    mov si, offset msg_memory_error
    call print
    jmp prompt_loop

memory_ready:
    call enable_a20
    jnc a20_enabled
    mov si, offset msg_a20_error
    call print
    jmp prompt_loop

a20_enabled:
    mov dword ptr [INFO_BASE], 0x4C424455
    mov word ptr [INFO_BASE + 4], 1
    mov al, byte ptr [boot_drive]
    mov byte ptr [INFO_BASE + 6], al
    mov ah, 0x03
    xor bh, bh
    int 0x10
    mov byte ptr [INFO_BASE + 7], dh
    mov dword ptr [INFO_BASE + 12], 0
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    .byte 0x66, 0xEA
    .long protected_entry
    .word 0x08

match_word:
    mov al, byte ptr [di]
    test al, al
    jz match_end
    cmp al, byte ptr [si]
    jne match_fail
    inc si
    inc di
    jmp match_word
match_end:
    mov al, byte ptr [si]
    test al, al
    jz match_ok
    cmp al, 0x20
    je match_ok
match_fail:
    mov al, 1
    test al, al
    ret
match_ok:
    xor al, al
    ret

read_line:
    mov di, offset line_buffer
    xor cx, cx
read_key:
    xor ax, ax
    int 0x16
    cmp al, 0x0D
    je read_done
    cmp al, 0x08
    je read_backspace
    cmp al, 0x20
    jb read_key
    cmp al, 0x7E
    ja read_key
    cmp cx, LINE_MAX
    jae read_key
    cmp al, 0x41
    jb store_char
    cmp al, 0x5A
    ja store_char
    add al, 0x20
store_char:
    mov byte ptr [di], al
    inc di
    inc cx
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp read_key
read_backspace:
    test cx, cx
    jz read_key
    dec di
    dec cx
    mov ax, 0x0E08
    mov bx, 0x0007
    int 0x10
    mov ax, 0x0E20
    int 0x10
    mov ax, 0x0E08
    int 0x10
    jmp read_key
read_done:
    mov byte ptr [di], 0
    mov si, offset msg_newline
    call print
    ret

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

tick_dots:
    pushad
    mov eax, dword ptr [0x046C]
    sub eax, dword ptr [dot_start]
    mov ecx, 1000
    mul ecx
    mov ecx, 18207
    div ecx
    cmp eax, BOOT_DOTS
    jbe tick_due_ready
    mov eax, BOOT_DOTS

tick_due_ready:
    mov dx, ax
    mov cx, word ptr [dots_printed]

tick_print:
    cmp cx, dx
    jae tick_done
    mov ax, 0x0E2E
    mov bx, 0x0007
    int 0x10
    inc cx
    jmp tick_print

tick_done:
    mov word ptr [dots_printed], cx
    popad
    ret

wait_dots:
    call tick_dots
    cmp word ptr [dots_printed], BOOT_DOTS
    jae wait_dots_done
    sti
    hlt
    jmp wait_dots

wait_dots_done:
    ret

init_disk:
    mov ah, 0x08
    mov dl, byte ptr [boot_drive]
    xor di, di
    mov es, di
    int 0x13
    jc init_disk_fail
    mov al, cl
    and al, 0x3F
    xor ah, ah
    mov word ptr [geo_spt], ax
    mov al, dh
    xor ah, ah
    inc ax
    mov word ptr [geo_heads], ax
    cmp word ptr [geo_spt], 0
    je init_disk_fail
    clc
    ret

init_disk_fail:
    stc
    ret

read_lba:
    pushad
    xor edx, edx
    movzx ecx, word ptr [geo_spt]
    div ecx
    inc dl
    mov byte ptr [chs_sector], dl
    xor edx, edx
    movzx ecx, word ptr [geo_heads]
    div ecx
    cmp eax, 1023
    ja read_lba_fail
    mov ch, al
    mov cl, ah
    shl cl, 6
    or cl, byte ptr [chs_sector]
    mov dh, dl
    mov dl, byte ptr [boot_drive]
    mov di, READ_RETRIES

read_lba_retry:
    mov ax, 0x0201
    int 0x13
    jnc read_lba_ok
    xor ax, ax
    int 0x13
    dec di
    jnz read_lba_retry

read_lba_fail:
    mov byte ptr [load_error], ERR_DISK
    popad
    stc
    ret

read_lba_ok:
    popad
    clc
    ret

load_inode:
    push si
    test eax, eax
    jz load_inode_bad
    cmp eax, dword ptr [inode_count]
    ja load_inode_bad
    dec eax
    mov edx, eax
    shr eax, 3
    and edx, 7
    shl edx, 6
    add eax, dword ptr [itable_start]
    add eax, VOL_START_LBA
    mov bx, BLK_BUF
    call read_lba
    jc load_inode_fail
    mov si, BLK_BUF
    add si, dx
    mov di, INODE_BUF
    mov cx, 16
    rep movsd
    pop si
    clc
    ret

load_inode_bad:
    mov byte ptr [load_error], ERR_FS

load_inode_fail:
    pop si
    stc
    ret

name_equal:
    push si
    push di
    push cx
    mov cx, 28

name_loop:
    mov al, byte ptr [si]
    cmp al, byte ptr [di]
    jne name_diff
    test al, al
    jz name_same
    inc si
    inc di
    loop name_loop

name_same:
    xor al, al
    jmp name_return

name_diff:
    mov al, 1
    test al, al

name_return:
    pop cx
    pop di
    pop si
    ret

dir_lookup:
    mov word ptr [comp_ptr], si
    mov byte ptr [ext_idx], 0

dir_extent:
    movzx di, byte ptr [ext_idx]
    shl di, 3
    mov eax, dword ptr [di + INODE_BUF + 32]
    mov ecx, dword ptr [di + INODE_BUF + 36]
    test ecx, ecx
    jz dir_not_found
    mov dword ptr [blk_cur], eax
    mov dword ptr [blk_left], ecx

dir_block:
    mov eax, dword ptr [blk_cur]
    add eax, VOL_START_LBA
    mov bx, DIR_BUF
    call read_lba
    jc dir_fail
    mov di, DIR_BUF
    mov cx, 16

dir_entry:
    cmp dword ptr [di], 0
    je dir_next_entry
    push di
    add di, 4
    mov si, word ptr [comp_ptr]
    call name_equal
    pop di
    jz dir_found

dir_next_entry:
    add di, 32
    loop dir_entry
    inc dword ptr [blk_cur]
    dec dword ptr [blk_left]
    jnz dir_block
    inc byte ptr [ext_idx]
    cmp byte ptr [ext_idx], 4
    jb dir_extent

dir_not_found:
    mov byte ptr [load_error], ERR_NOT_FOUND

dir_fail:
    stc
    ret

dir_found:
    mov edx, dword ptr [di]
    mov si, word ptr [comp_ptr]

dir_skip:
    lodsb
    test al, al
    jnz dir_skip
    mov eax, edx
    clc
    ret

load_kernel:
    mov byte ptr [load_error], ERR_DISK
    call init_disk
    jc load_kernel_fail
    mov eax, VOL_START_LBA
    mov bx, SB_BUF
    call read_lba
    jc load_kernel_fail
    mov byte ptr [load_error], ERR_FS
    cmp dword ptr [SB_BUF], UDFS_MAGIC_VALUE
    jne load_kernel_fail
    cmp word ptr [SB_BUF + 4], 1
    jne load_kernel_fail
    cmp dword ptr [SB_BUF + 8], 512
    jne load_kernel_fail
    mov eax, dword ptr [SB_BUF + 36]
    mov dword ptr [itable_start], eax
    mov eax, dword ptr [SB_BUF + 16]
    mov dword ptr [inode_count], eax
    mov eax, dword ptr [SB_BUF + 48]
    mov si, offset path_kernel

resolve_next:
    call load_inode
    jc load_kernel_fail
    cmp byte ptr [si], 0
    je kernel_found
    cmp word ptr [INODE_BUF], 2
    jne resolve_not_found
    call dir_lookup
    jc load_kernel_fail
    jmp resolve_next

resolve_not_found:
    mov byte ptr [load_error], ERR_NOT_FOUND
    jmp load_kernel_fail

kernel_found:
    mov byte ptr [load_error], ERR_INVALID
    cmp word ptr [INODE_BUF], 1
    jne load_kernel_fail
    cmp dword ptr [INODE_BUF + 12], 0
    jne load_kernel_fail
    mov ecx, dword ptr [INODE_BUF + 8]
    cmp ecx, KERNEL_MAX_BYTES
    ja load_kernel_fail
    add ecx, 511
    shr ecx, 9
    jz load_kernel_fail
    mov dword ptr [file_sectors], ecx
    mov dword ptr [sectors_left], ecx
    mov word ptr [cur_seg], KERNEL_TEMP_SEG
    mov byte ptr [ext_idx], 0

file_extent:
    cmp dword ptr [sectors_left], 0
    je load_kernel_done
    cmp byte ptr [ext_idx], 4
    jae load_kernel_fail
    movzx di, byte ptr [ext_idx]
    shl di, 3
    mov eax, dword ptr [di + INODE_BUF + 32]
    mov ecx, dword ptr [di + INODE_BUF + 36]
    test ecx, ecx
    jz load_kernel_fail
    mov dword ptr [blk_cur], eax
    mov dword ptr [blk_left], ecx

file_block:
    mov eax, dword ptr [blk_cur]
    add eax, VOL_START_LBA
    mov bx, word ptr [cur_seg]
    mov es, bx
    xor bx, bx
    call read_lba
    jc load_kernel_fail
    add word ptr [cur_seg], 0x20
    inc dword ptr [blk_cur]
    dec dword ptr [sectors_left]
    call tick_dots
    dec dword ptr [blk_left]
    jz file_next_extent
    cmp dword ptr [sectors_left], 0
    jne file_block
    jmp load_kernel_done

file_next_extent:
    inc byte ptr [ext_idx]
    jmp file_extent

load_kernel_done:
    xor ax, ax
    mov es, ax
    clc
    ret

load_kernel_fail:
    xor ax, ax
    mov es, ax
    stc
    ret

detect_memory:
    mov dword ptr [INFO_MMAP_COUNT], 0
    mov di, INFO_MMAP_ENTRIES
    xor ebx, ebx

e820_next:
    mov dword ptr [di + 20], 1
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    int 0x15
    jc e820_carry
    cmp eax, 0x534D4150
    jne e820_fail
    cmp ecx, 20
    jb e820_skip
    inc dword ptr [INFO_MMAP_COUNT]
    add di, 24

e820_skip:
    test ebx, ebx
    jz e820_done
    cmp dword ptr [INFO_MMAP_COUNT], MMAP_MAX
    jb e820_next

e820_done:
    cmp dword ptr [INFO_MMAP_COUNT], 0
    je e820_fail
    clc
    ret

e820_carry:
    cmp dword ptr [INFO_MMAP_COUNT], 0
    jne e820_done

e820_fail:
    stc
    ret

a20_check:
    push ds
    push es
    push si
    push di
    xor ax, ax
    mov ds, ax
    mov ax, 0xFFFF
    mov es, ax
    mov si, 0x0500
    mov di, 0x0510
    mov al, byte ptr ds:[si]
    mov ah, byte ptr es:[di]
    push ax
    mov byte ptr ds:[si], 0x00
    mov byte ptr es:[di], 0xFF
    cmp byte ptr ds:[si], 0xFF
    pop ax
    mov byte ptr es:[di], ah
    mov byte ptr ds:[si], al
    pop di
    pop si
    pop es
    pop ds
    ret

enable_a20:
    call a20_check
    jnz a20_ready
    mov ax, 0x2401
    int 0x15
    call a20_check
    jnz a20_ready
    in al, 0x92
    or al, 0x02
    and al, 0xFE
    out 0x92, al
    mov cx, 0x1000

a20_wait:
    call a20_check
    jnz a20_ready
    loop a20_wait
    stc
    ret

a20_ready:
    clc
    ret

msg_prompt:
    .asciz "Boot> "

msg_newline:
    .asciz "\r\n"

msg_unknown:
    .asciz "UNKNOWN COMMAND\r\n"

msg_loading:
    .asciz "STARTING UDOS KERNEL"

msg_ok:
    .asciz "OK\r\n"

msg_failed:
    .asciz "FAILED\r\n"

msg_err_disk:
    .asciz "UDBL: DISK READ ERROR\r\n"

msg_err_fs:
    .asciz "UDBL: BAD FILE SYSTEM\r\n"

msg_err_not_found:
    .asciz "UDBL: KERNEL NOT FOUND\r\n"

msg_err_invalid:
    .asciz "UDBL: KERNEL FILE INVALID\r\n"

msg_memory_error:
    .asciz "UDBL: MEMORY MAP ERROR\r\n"

msg_a20_error:
    .asciz "UDBL: A20 ERROR\r\n"

cmd_start:
    .asciz "start"

path_kernel:
    .asciz "sys"
    .asciz "boot"
    .asciz "kernel"
    .byte 0

boot_drive:
    .byte 0

load_error:
    .byte 0

ext_idx:
    .byte 0

chs_sector:
    .byte 0

flags_ptr:
    .word 0

dots_printed:
    .word 0

geo_spt:
    .word 0

geo_heads:
    .word 0

cur_seg:
    .word 0

comp_ptr:
    .word 0

dot_start:
    .long 0

itable_start:
    .long 0

inode_count:
    .long 0

blk_cur:
    .long 0

blk_left:
    .long 0

sectors_left:
    .long 0

file_sectors:
    .long 0

.align 8
gdt:
    .quad 0x0000000000000000
    .quad 0x00CF9A000000FFFF
    .quad 0x00CF92000000FFFF
    .quad 0x00AF9A000000FFFF
gdt_end:

gdt_descriptor:
    .word gdt_end - gdt - 1
    .long gdt

line_buffer:
    .space LINE_MAX + 1

.code32
protected_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, STACK32_TOP
    cld
    mov esi, KERNEL_TEMP_ADDR
    mov edi, KERNEL_DEST
    mov ecx, dword ptr [file_sectors]
    shl ecx, 7
    rep movsd
    mov edi, PML4_ADDR
    xor eax, eax
    mov ecx, 0xC00
    rep stosd
    mov dword ptr [PML4_ADDR], PDPT_ADDR + 3
    mov dword ptr [PDPT_ADDR], PD_ADDR + 3
    mov edi, PD_ADDR
    mov eax, 0x83
    mov ecx, 512

pd_fill:
    mov dword ptr [edi], eax
    add edi, 8
    add eax, 0x200000
    loop pd_fill
    mov eax, PML4_ADDR
    mov cr3, eax
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    .byte 0xEA
    .long long_entry
    .word 0x18

.code64
long_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, STACK64_TOP
    mov edi, INFO_BASE
    mov eax, KERNEL_DEST
    jmp rax
