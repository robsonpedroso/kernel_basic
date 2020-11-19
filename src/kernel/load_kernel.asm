[bits 32]
global kernel_sleep
global kernel_shutdown
[extern main]

loadkernel:
    call main
    jmp $

kernel_sleep:
    push ebp
    mov  ebp, esp
    mov ecx,dword [ebp+8]
    imul ecx,1000

kernel_shutdown:
    mov ax, 5300h
    xor bx, bx
    int 15h