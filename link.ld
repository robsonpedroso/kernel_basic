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
   .bss  : { *(.bss)  }
   /* Unwind/debug metadata this freestanding kernel never uses -- besides
      being dead weight, letting it land as an orphan is what caused the
      instability above. */
   /DISCARD/ : { *(.eh_frame*) *(.comment) *(.note.*) }
 }

/* The heap starts at 0x60000 (see heap.c) -- if the kernel image ever
   grows past that, it silently overlaps the heap at runtime instead of
   failing loudly, so catch it here at link time instead.

   This is also the operative early-warning for the boot-time IDE PIO
   loader's own budget (IDE_SECTORS in src/boot/bootloader.asm, currently
   800 sectors / 400KB, loaded with no BIOS involvement so no BIOS transfer
   ceiling applies -- see README.md's "Orçamento de boot" section). This
   0x60000 (~384KB of .text+.rodata+.data+.bss combined) assert fires well
   before that 400KB disk-read budget would ever be threatened, so there is
   normally no need to touch bootloader.asm at all when the kernel grows --
   just this assert. */
ASSERT(. <= 0x60000, "kernel image has grown past 0x60000 and now overlaps the heap (see heap.c HEAP_START) -- move the heap or trim the image")