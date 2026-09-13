# apps/games/src/<game> sources are vendored third-party engine ports
# (doomgeneric today, ~60-70 upstream .c files once fully vendored) rather
# than kernel code -- hand-listing them one `gcc -c` line at a time, like
# every other source in this file, would be unmaintainable and a constant
# "forgot to add the new file" trap. This is the one pattern-rule exception
# to the rest of the Makefile's fully-explicit style, scoped only to this
# subtree.
DOOM_SRC_DIRS := apps/games/src/doom apps/games/src/doom/doomgeneric

# doomgeneric/ is vendored in full (upstream tree, unmodified) rather than
# pruned, so future backends/updates can diff cleanly against upstream --
# but only the portable engine (upstream's own default Makefile SRC_DOOM
# list, minus its X11 backend) actually gets built here. Excluded: other
# platforms' backend .c files (we provide doomgeneric_rsystemos.c instead)
# and the SDL/Allegro/GUS sound backends (no sound driver exists in this
# kernel at all -- see doomgeneric_rsystemos.c's stubbed DG_* hooks).
DOOM_EXCLUDE := $(addprefix apps/games/src/doom/doomgeneric/, \
	doomgeneric_sdl.c doomgeneric_allegro.c doomgeneric_emscripten.c \
	doomgeneric_linuxvt.c doomgeneric_soso.c doomgeneric_sosox.c \
	doomgeneric_win.c doomgeneric_xlib.c \
	i_sdlmusic.c i_sdlsound.c i_allegromusic.c i_allegrosound.c gusconf.c)

DOOM_SRCS     := $(filter-out $(DOOM_EXCLUDE),$(wildcard $(addsuffix /*.c,$(DOOM_SRC_DIRS))))
DOOM_OBJS     := $(patsubst %.c,dist/doom_%.o,$(notdir $(DOOM_SRCS)))
VPATH         := $(DOOM_SRC_DIRS)

DOOM_SHIM_INCLUDE := apps/games/src/doom/libc_shim/include

# Phase 2 (video): pin doomgeneric's output buffer to Doom's own native
# 320x200 instead of the upstream default 640x400 (doomgeneric.h). This
# makes i_video.c's I_InitGraphics compute fb_scaling=1 with zero
# x/y_offset letterboxing -- the simplest, fastest case for
# doomgeneric_rsystemos.c's DG_DrawFrame to blit (see its own comment):
# straight 1:1, no scaling math, and 1/4 the pixels of the 640x400 default
# to run through this kernel's slow port-I/O-per-pixel VGA writes.
DOOM_RES_DEFS := -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200

dist/doom_%.o: %.c
	gcc -fno-pie -fno-stack-protector -Os -I $(DOOM_SHIM_INCLUDE) $(DOOM_RES_DEFS) -c $< -o $@ -m32

all: $(DOOM_OBJS)
	nasm -f bin src/boot/bootloader.asm -o dist/bootloader.bin
	gcc -fno-pie -fno-stack-protector -c src/kernel/kernel.c -o dist/kernel.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/pic.c -o dist/pic.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/gdt.c -o dist/gdt.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/idt.c -o dist/idt.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/isr.c -o dist/isr.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/serial.c -o dist/serial.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/event.c -o dist/event.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/timer.c -o dist/timer.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/thread.c -o dist/thread.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/storage_thread.c -o dist/storage_thread.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/mouse.c -o dist/mouse.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/fs.c -o dist/fs.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/wad_disk.c -o dist/wad_disk.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/cc.c -o dist/cc.o -m32
	gcc -fno-pie -fno-stack-protector -c src/lib/stdlib.c -o dist/stdlib.o -m32
	gcc -fno-pie -fno-stack-protector -c src/lib/string.c -o dist/string.o -m32
	gcc -fno-pie -fno-stack-protector -c src/lib/heap.c -o dist/heap.o -m32
	gcc -fno-pie -fno-stack-protector -c src/drivers/video.c -o dist/video.o -m32
	gcc -fno-pie -fno-stack-protector -c src/drivers/keyboard.c -o dist/keyboard.o -m32
	gcc -fno-pie -fno-stack-protector -c src/drivers/ide.c -o dist/ide.o -m32
	gcc -fno-pie -fno-stack-protector -c src/drivers/rtc.c -o dist/rtc.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/rect.c -o dist/rect.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/widget.c -o dist/widget.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/window.c -o dist/window.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/cursor.c -o dist/cursor.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/icon.c -o dist/icon.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/textbox.c -o dist/textbox.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/editbuf.c -o dist/editbuf.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/lineedit.c -o dist/lineedit.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/scrollbar.c -o dist/scrollbar.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/listbox.c -o dist/listbox.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/treeview.c -o dist/treeview.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/splitter.c -o dist/splitter.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/menubar.c -o dist/menubar.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/taskbar.c -o dist/taskbar.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/confirm.c -o dist/confirm.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/wm.c -o dist/wm.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/apps/info.c -o dist/app_info.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/apps/terminal.c -o dist/app_terminal.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/apps/calculator.c -o dist/app_calculator.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/apps/program_manager.c -o dist/app_program_manager.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/apps/text_editor.c -o dist/app_text_editor.o -m32
	gcc -fno-pie -fno-stack-protector -c src/gui/apps/file_manager.c -o dist/app_file_manager.o -m32
	nasm -f elf32 src/kernel/load_kernel.asm -o dist/load_kernel.o
	nasm -f elf32 src/kernel/isr.asm -o dist/isr_stubs.o
	nasm -f elf32 src/kernel/context_switch.asm -o dist/context_switch.o
	ld -melf_i386 -o dist/kernel.bin -Ttext 0x1000 dist/load_kernel.o dist/kernel.o dist/pic.o dist/gdt.o dist/idt.o dist/isr.o dist/isr_stubs.o dist/serial.o dist/event.o dist/timer.o dist/thread.o dist/context_switch.o dist/storage_thread.o dist/mouse.o dist/fs.o dist/wad_disk.o dist/cc.o dist/stdlib.o dist/string.o dist/heap.o dist/video.o dist/keyboard.o dist/ide.o dist/rtc.o dist/rect.o dist/widget.o dist/window.o dist/cursor.o dist/icon.o dist/textbox.o dist/editbuf.o dist/lineedit.o dist/scrollbar.o dist/listbox.o dist/treeview.o dist/splitter.o dist/menubar.o dist/taskbar.o dist/confirm.o dist/wm.o dist/app_info.o dist/app_terminal.o dist/app_calculator.o dist/app_program_manager.o dist/app_text_editor.o dist/app_file_manager.o $(DOOM_OBJS) --oformat binary -T link.ld
	# The image is created zero-filled ONCE. Rebuilds must not recreate it:
	# everything from LBA 801 on is the filesystem region (fs.c), and it has
	# to survive `make all`, otherwise persistence only holds until the next
	# build. 32768 sectors (16MiB) instead of the old 2880 (1.44MiB): LBA
	# 801..2879 is still the fs.c region, unchanged; LBA 2880+ is reserved
	# for the Doom WAD blob (see wad_disk.h WAD_LBA) below. An image built
	# before this change is still only 1.44MiB on disk and needs `make
	# distclean && make all` to pick up the new size, same as any other
	# on-disk-layout change documented in README.md's "Orçamento de boot".
	test -f dist/rSystemOS-v0_1.img || dd if=/dev/zero of=dist/rSystemOS-v0_1.img bs=512 count=32768 status=none
	dd if=dist/bootloader.bin of=dist/rSystemOS-v0_1.img bs=512 seek=0 conv=notrunc status=none
	# Zero the whole 4096-sector kernel budget (see bootloader.asm
	# IDE_SECTORS) before writing the new kernel. The bootloader always
	# reads all IDE_SECTORS sectors into RAM, and `ld --oformat binary`
	# does not emit .bss -- the kernel's .bss is zero at runtime ONLY
	# because the sectors behind kernel.bin are zero. Leaving a larger
	# previous build's tail there loads garbage straight into .bss (e.g.
	# heap.c's heap_initialized), which is an instant, very confusing crash.
	dd if=/dev/zero of=dist/rSystemOS-v0_1.img bs=512 seek=1 count=4096 conv=notrunc status=none
	dd if=dist/kernel.bin of=dist/rSystemOS-v0_1.img bs=512 seek=1 conv=notrunc status=none
	# Doom IWAD blob, if present. Not vendored into the repo (see
	# apps/games/src/doom/wads/.gitignore) -- drop doom1.wad there yourself
	# before building. Idempotent and safe on every rebuild: LBA 2880+ is
	# never touched by fs.c's fs_format()/bump allocator, so there's no
	# collision or persistence hazard here the way there is with the
	# kernel-zeroing step above.
	test -f apps/games/src/doom/wads/doom1.wad && dd if=apps/games/src/doom/wads/doom1.wad of=dist/rSystemOS-v0_1.img bs=512 seek=6176 conv=notrunc status=none || true
	rm -f dist/*.bin dist/*.o

clean:
	rm -f dist/*.o dist/*.bin

# Also throws away the filesystem region -- the next `make all` recreates
# the image zero-filled, and fs_init() reformats it on the bad magic.
distclean: clean
	rm -f dist/rSystemOS-v0_1.img

exec:
	make all
	# -m 32: explicit RAM size. There's no E820/memory-map probing in this
	# kernel, so nothing validates that physical RAM actually backs the
	# heap's 0x300000..0xB00000 range (see heap.c) before it starts handing
	# out pointers into it -- pin it rather than rely on qemu's
	# version-dependent default.
	# -serial stdio: prints the kernel's serial_write() log (boot status,
	# and isr.c's "*** KERNEL PANIC ***"/"EXCEPTION ... eip=" line on a
	# crash) straight to this terminal, so a freeze/panic always leaves a
	# copyable trail here instead of only being visible on-screen.
	qemu-system-x86_64 -m 32 -hda dist/rSystemOS-v0_1.img -serial stdio
