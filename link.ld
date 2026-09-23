OUTPUT_FORMAT(elf32-i386)
SECTIONS
 {
   . = 0x1000;
   .text : { *(.text) }
   /* Explicit, instead of letting ld's orphan-section placement decide --
      that placement is not stable across output modes (observed to size
      .eh_frame differently, and therefore shift .data, between a plain
      ELF link and a direct `ld --oformat binary` link of the exact same
      objects), which silently desynced code addresses from the flat
      binary's actual byte layout. */
   .rodata : { *(.rodata*) }
   .data : { *(.data) }
   /* .bss placed past 0xA0000 -- the legacy VGA framebuffer window this
      kernel's own video.c writes through (FRAMEBUFFER, see
      src/drivers/video.c) for every pixel draw call. A .bss variable
      landing in 0xA0000..0xBFFFF isn't corrupted once at boot; it's
      silently overwritten by ordinary screen drawing on every repaint
      from then on. Jumping to 0x100000 (1MiB, "extended memory" in this
      project's own vocabulary -- see heap.c/bootloader.asm, already
      relied on for the runtime stack/heap past this same point) skips
      the VGA hole entirely and leaves .bss's footprint well under the
      0x200000 stack boundary asserted below. Costs no file bytes:
      `ld --oformat binary` (see Makefile) never emits .bss content
      regardless of its address, so this gap does not enlarge kernel.bin
      or the boot-time IDE_SECTORS budget. */
   . = 0x100000;
   .bss  : { *(.bss)  }
   /* Unwind/debug metadata this freestanding kernel never uses -- besides
      being dead weight, letting it land as an orphan is what caused the
      instability above. */
   /DISCARD/ : { *(.eh_frame*) *(.comment) *(.note.*) }
 }

/* The runtime stack now lives at 0x200000..0x300000 (see bootloader.asm's
   init_pm), relocated from the original 0x90000 once the kernel image
   outgrew the space below it. If the kernel image ever grows into the
   stack region, stack and .data/.bss would silently corrupt each other at
   runtime instead of failing loudly, so catch it here at link time
   instead.

   This is also the operative early-warning for the boot-time IDE PIO
   loader's own budget (IDE_SECTORS in src/boot/bootloader.asm, currently
   4096 sectors / 2MB, loaded with no BIOS involvement so no BIOS transfer
   ceiling applies -- see README.md's "Orçamento de boot" section). If the
   image ever needs to grow past this 0x200000 (2MiB) assert, IDE_SECTORS
   needs bumping in lockstep (see fs.h's own build notes for the exact
   procedure: bump IDE_SECTORS, shift FS_SUPER_LBA/FS_TABLE_LBA/
   FS_DATA_LBA/FS_END_LBA in fs.h by the same delta, then
   `make distclean && make all`). */
ASSERT(. <= 0x200000, "kernel image has grown past 0x200000 and is encroaching on the runtime stack region at 0x200000..0x300000 (see bootloader.asm) -- move the stack or trim the image")