.section .rodata
.balign 16
.globl user_hello_start
user_hello_start:
.incbin "hello.bin"
.globl user_hello_end
user_hello_end:

.section .note.GNU-stack,"",@progbits
