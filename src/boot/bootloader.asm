[org 0x7c00]

jmp load

; This sector no longer pretends to be a FAT12 floppy boot sector (BPB) --
; we're a plain MBR-style boot sector on a raw IDE disk image now, and BIOS
; doesn't require a BPB or partition table to boot us: it just loads this
; 512-byte sector to 0x7c00 and jumps here. That freed up the room this
; file needed for the IDE PIO loader below.

load:
    ; DS/ES stay at segment 0 -- everything below (and later lgdt) addresses
    ; memory relative to them, and BIOS only guarantees CS:IP on entry.
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    MOV EBP, 0x9000     ; small real-mode stack, only needed for the INT 10h
    MOV ESP, EBP         ; call below (interrupts push flags/cs/ip on it)

    ; Enable VGA Mode 12h (640x480x16, planar) so the kernel can draw pixels.
    mov ax, 0x0012
    int 10h

    cli

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    jmp gdt_codeSeg:init_pm

[bits 32]

init_pm:
    MOV AX, gdt_dataSeg                     ; Update segments for protected mode as defined in GDT
    MOV DS, AX
    MOV SS, AX
    MOV ES, AX

    ; Runtime kernel stack, set well above the loaded kernel image (which
    ; grows up from 0x1000) so neither can grow into the other. 0x90000
    ; leaves ~500KB+ of headroom -- comfortable for a long time to come.
    MOV EBP, 0x90000
    MOV ESP, EBP

    call ide_load_kernel   ; pull the kernel body in via raw IDE PIO (no BIOS
                            ; involved, so the ~24KB BIOS INT13h ceiling this
                            ; loader used to hit doesn't apply anymore)

    call 0x1000             ; jump to load kernel asm

    jmp $

; Loads IDE_SECTORS sectors starting at LBA 1 (the sector right after this
; boot sector) into IDE_DEST, growing upward -- one sector per command,
; primary ATA channel/master drive/LBA28. Standard OSDev-wiki PIO technique;
; unlike BIOS INT13h this talks to the controller directly, so it isn't
; subject to any BIOS-side transfer limit.
IDE_DATA    equ 0x1F0
IDE_SECCNT  equ 0x1F2
IDE_LBA_LO  equ 0x1F3
IDE_LBA_MID equ 0x1F4
IDE_LBA_HI  equ 0x1F5
IDE_DRVHEAD equ 0x1F6
IDE_CMD     equ 0x1F7   ; status on read

IDE_SECTORS equ 800     ; 800*512 = 400KB budget for the kernel image
IDE_DEST    equ 0x1000

; This boot sector itself still lives (and is still executing!) at
; 0x7C00-0x7DFF. With a load target this big, the destination pointer
; always walks straight through that range once the kernel image is big
; enough -- so the ONE sector whose real destination falls there gets
; redirected here instead of overwriting the code/variables currently
; reading it. Past the runtime stack (ESP starts at 0x90000 and grows
; down), well before the VGA framebuffer at 0xA0000, so nothing else uses
; it. load_kernel.asm copies it into its real place (0x7C00) as the very
; first thing the kernel does, once it's safe (no longer executing from
; there) -- see the comment there for why a plain "skip this sector"
; would corrupt everything loaded after it instead.
IDE_SCRATCH equ 0x91000

ide_load_kernel:
    pushad

    mov dword [ide_lba], 1
    mov dword [ide_dest_ptr], IDE_DEST
    mov dword [ide_left], IDE_SECTORS

.next_sector:
    cmp dword [ide_left], 0
    je .done

    ; edi = where THIS sector's data actually gets written. Normally that's
    ; [ide_dest_ptr]; redirect to scratch for the one sector that would
    ; land on the currently-executing boot sector. Crucially, [ide_dest_ptr]
    ; itself is left untouched either way, so it keeps advancing by exactly
    ; 512 bytes/sector with no permanent shift -- only this one sector's
    ; placement is deferred, nothing after it moves.
    mov edi, [ide_dest_ptr]
    cmp edi, 0x7C00
    jb .dest_ok
    cmp edi, 0x7E00
    jae .dest_ok
    mov edi, IDE_SCRATCH
.dest_ok:

    mov dx, IDE_CMD
.wait_bsy:
    in al, dx
    test al, 0x80            ; BSY
    jnz .wait_bsy

    mov dx, IDE_SECCNT
    mov al, 1
    out dx, al

    mov eax, [ide_lba]
    mov dx, IDE_LBA_LO
    out dx, al

    mov eax, [ide_lba]
    shr eax, 8
    mov dx, IDE_LBA_MID
    out dx, al

    mov eax, [ide_lba]
    shr eax, 16
    mov dx, IDE_LBA_HI
    out dx, al

    mov eax, [ide_lba]
    shr eax, 24
    and al, 0x0F
    or al, 0xE0               ; master drive, LBA mode
    mov dx, IDE_DRVHEAD
    out dx, al

    mov dx, IDE_CMD
    mov al, 0x20               ; READ SECTORS
    out dx, al

.wait_drq:
    in al, dx
    test al, 0x80             ; BSY
    jnz .wait_drq
    test al, 0x01             ; ERR
    jnz ide_error
    test al, 0x08             ; DRQ
    jz .wait_drq

    ; edi already holds the correct destination (normal or scratch, set
    ; above before the read command was issued).
    mov dx, IDE_DATA
    mov ecx, 256               ; 256 words = 512 bytes
    cld
    rep insw

    add dword [ide_dest_ptr], 512
    inc dword [ide_lba]
    dec dword [ide_left]
    jmp .next_sector

.done:
    popad
    ret

ide_error:
    cli
.halt:
    hlt
    jmp .halt

ide_lba:      dd 0
ide_dest_ptr: dd 0
ide_left:     dd 0

; Global descriptor table for 32-bit protected mode
gdt_start:                              ; Start of global descriptor table
    gdt_null:                           ; Null descriptor chunk
        dd 0x00
        dd 0x00
    gdt_code:                           ; Code descriptor chunk
        dw 0xFFFF
        dw 0x0000
        db 0x00
        db 0x9A
        db 0xCF
        db 0x00
    gdt_data:                           ; Data descriptor chunk
        dw 0xFFFF
        dw 0x0000
        db 0x00
        db 0x92
        db 0xCF
        db 0x00
    gdt_end:                            ; Bottom of table
    gdt_descriptor:                         ; Table descriptor
        dw gdt_end - gdt_start - 1          ; Size of table
        dd gdt_start                        ; Start point of table

gdt_codeSeg equ gdt_code - gdt_start    ; Offset of code segment from start
gdt_dataSeg equ gdt_data - gdt_start    ; Offset of data segment from start

; Tail

times 510 -( $ - $$ ) db 0
dw 0xaa55
