BITS 64
default rel

section .text
global _start
extern kmain

_start:
    cli
    cld
    call kmain

.hang:
    hlt
    jmp .hang
