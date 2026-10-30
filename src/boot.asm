; AQ OS - Multiboot v1 header requesting a linear 640x480x32 mode
BITS 32
section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00000007
    dd -(0x1BADB002 + 0x00000007)
    dd 0              ; mode_type: 0 = linear graphics
    dd 640            ; width
    dd 480            ; height
    dd 32             ; bits per pixel

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top
    xor ebp, ebp
    push ebx
    push eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
