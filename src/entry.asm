BITS 64
default rel

section .text
global _start
extern kmain

_start:
    cli
    cld
    ; Enable SSE2 for the isolated stb_truetype translation unit. No scheduler
    ; exists yet, so there is no cross-task FPU/SIMD state to preserve.
    mov rax, cr0
    btr rax, 2               ; CR0.EM = 0
    btr rax, 3               ; CR0.TS = 0
    bts rax, 1               ; CR0.MP = 1
    mov cr0, rax
    mov rax, cr4
    bts rax, 9               ; CR4.OSFXSR
    bts rax, 10              ; CR4.OSXMMEXCPT
    mov cr4, rax
    call kmain

.hang:
    hlt
    jmp .hang
