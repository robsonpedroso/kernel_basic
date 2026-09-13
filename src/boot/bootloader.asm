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

    ; Fast A20 gate (System Control Port A, port 0x92, bit 1). Required now
    ; that the heap lives past the 1MiB mark (see heap.c HEAP_START) --
    ; without this, addresses up there silently wrap back into low memory
    ; instead of failing loudly. Broadly supported by QEMU's emulated
    ; chipset, the only tested target here (see Makefile's `exec`); the
    ; keyboard-controller method (port 0x64/0x60) is the documented
    ; fallback if a different target ever needs it.
    in al, 0x92
    or al, 2
    out 0x92, al

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

    ; Runtime kernel stack. Relocated from the original 0x90000 into
    ; extended memory (requires the A20 gate above) once the vendored Doom
    ; engine's own .bss (visplanes/openings/zlight/etc render tables, see
    ; the Phase 1.5 link-probe notes in apps/games/src/doom/) pushed the
    ; kernel image itself well past the old ~544KB budget between 0x1000
    ; and 0x90000. New layout, each region comfortably oversized for
    ; today's actual usage: kernel image 0x1000..0x200000 (see link.ld's
    ; ASSERT), stack region 0x200000..0x300000 (this base, growing down),
    ; heap 0x300000..0xB00000 (see heap.c HEAP_START/HEAP_SIZE).
    MOV EBP, 0x2F0000
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

; 4096*512 = 2MB budget -- raised from the original 800 (400KB) once the
; vendored Doom engine (apps/games/src/doom/) measured at ~890KB of linked
; .text+.rodata+.data+.bss (see link.ld's ASSERT, which caps the image at
; exactly this same 2MB so the two constants can't silently drift apart).
; fs.h's FS_SUPER_LBA/FS_TABLE_LBA/FS_DATA_LBA/FS_END_LBA and wad_disk.h's
; WAD_LBA all shifted by the same delta (3296 sectors) to stay right after
; this budget -- see those headers' comments.
IDE_SECTORS equ 4096
IDE_DEST    equ 0x1000

; This boot sector itself still lives (and is still executing!) at
; 0x7C00-0x7DFF. With a load target this big, the destination pointer
; always walks straight through that range once the kernel image is big
; enough -- so the ONE sector whose real destination falls there gets
; redirected here instead of overwriting the code/variables currently
; reading it. This address is only ever touched transiently during boot,
; well before the runtime stack/heap (now up in extended memory, see
; init_pm's comment) or anything else is live, so its exact placement
; relative to them doesn't matter -- just that nothing else uses it, which
; holds since it sits below the 0xA0000 VGA framebuffer, untouched by
; either the old or new memory layout. load_kernel.asm copies it into its
; real place (0x7C00) as the very first thing the kernel does, once it's
; safe (no longer executing from there) -- see the comment there for why a
; plain "skip this sector" would corrupt everything loaded after it
; instead.
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
