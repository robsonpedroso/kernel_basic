[bits 32]
global kernel_sleep
global kernel_shutdown
[extern main]

loadkernel:
    ; Copy back the one 512-byte sector bootloader.asm's ide_load_kernel had
    ; to redirect into scratch space during boot: its real destination,
    ; 0x7C00-0x7DFF, is the boot sector's own code, which was still
    ; executing at the time. Safe now -- we run from here (0x1000+), not
    ; from there -- and necessary, since without this fixup that 512 bytes
    ; of the kernel image would simply never reach its linked address.
    mov esi, 0x91000
    mov edi, 0x7C00
    mov ecx, 512
    cld
    rep movsb

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