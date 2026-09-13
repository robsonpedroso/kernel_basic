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

/* The runtime stack now lives at 0x200000..0x300000 (see bootloader.asm's
   init_pm), relocated from the original 0x90000 once measuring the real
   vendored Doom engine (apps/games/src/doom/) showed it needs ~890KB of
   .text+.rodata+.data+.bss combined -- almost entirely renderer lookup
   tables (visplanes, openings, zlight, etc, all sized for classic Doom's
   fixed 320x200 internal resolution, see i_video.h SCREENWIDTH/HEIGHT;
   these are NOT affected by doomgeneric_rsystemos.c's own output
   resolution). If the kernel image ever grows into the stack region,
   stack and .data/.bss would silently corrupt each other at runtime
   instead of failing loudly, so catch it here at link time instead.

   This is also the operative early-warning for the boot-time IDE PIO
   loader's own budget (IDE_SECTORS in src/boot/bootloader.asm, currently
   800 sectors / 400KB, loaded with no BIOS involvement so no BIOS transfer
   ceiling applies -- see README.md's "Orçamento de boot" section). Unlike
   before, this 0x200000 (2MiB) assert does NOT fire well ahead of that
   400KB disk-read budget -- the measured ~890KB need already exceeds it,
   so IDE_SECTORS needs bumping in lockstep with any further image growth,
   not just this assert (see apps/games/src/doom/'s own build notes for
   the exact procedure: bump IDE_SECTORS, shift FS_SUPER_LBA/FS_TABLE_LBA/
   FS_DATA_LBA/FS_END_LBA and WAD_LBA in fs.h/wad_disk.h by the same delta,
   then `make distclean && make all`). */
ASSERT(. <= 0x200000, "kernel image has grown past 0x200000 and is encroaching on the runtime stack region at 0x200000..0x300000 (see bootloader.asm) -- move the stack or trim the image")