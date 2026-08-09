all:
	nasm -f bin src/boot/bootloader.asm -o dist/bootloader.bin
	gcc -fno-pie -fno-stack-protector -c src/kernel/kernel.c -o dist/kernel.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/pic.c -o dist/pic.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/gdt.c -o dist/gdt.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/idt.c -o dist/idt.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/isr.c -o dist/isr.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/serial.c -o dist/serial.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/event.c -o dist/event.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/timer.c -o dist/timer.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/mouse.c -o dist/mouse.o -m32
	gcc -fno-pie -fno-stack-protector -c src/kernel/fs.c -o dist/fs.o -m32
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
	ld -melf_i386 -o dist/kernel.bin -Ttext 0x1000 dist/load_kernel.o dist/kernel.o dist/pic.o dist/gdt.o dist/idt.o dist/isr.o dist/isr_stubs.o dist/serial.o dist/event.o dist/timer.o dist/mouse.o dist/fs.o dist/cc.o dist/stdlib.o dist/string.o dist/heap.o dist/video.o dist/keyboard.o dist/ide.o dist/rtc.o dist/rect.o dist/widget.o dist/window.o dist/cursor.o dist/icon.o dist/textbox.o dist/editbuf.o dist/lineedit.o dist/scrollbar.o dist/listbox.o dist/treeview.o dist/splitter.o dist/menubar.o dist/taskbar.o dist/confirm.o dist/wm.o dist/app_info.o dist/app_terminal.o dist/app_calculator.o dist/app_program_manager.o dist/app_text_editor.o dist/app_file_manager.o --oformat binary -T link.ld
	# The image is created zero-filled ONCE. Rebuilds must not recreate it:
	# everything from LBA 801 on is the filesystem region (fs.c), and it has
	# to survive `make all`, otherwise persistence only holds until the next
	# build.
	test -f dist/rSystemOS-v0_1.img || dd if=/dev/zero of=dist/rSystemOS-v0_1.img bs=512 count=2880 status=none
	dd if=dist/bootloader.bin of=dist/rSystemOS-v0_1.img bs=512 seek=0 conv=notrunc status=none
	# Zero the whole 800-sector kernel budget before writing the new kernel.
	# The bootloader always reads all 800 sectors into RAM, and `ld --oformat
	# binary` does not emit .bss -- the kernel's .bss is zero at runtime ONLY
	# because the sectors behind kernel.bin are zero. Leaving a larger
	# previous build's tail there loads garbage straight into .bss (e.g.
	# heap.c's heap_initialized), which is an instant, very confusing crash.
	dd if=/dev/zero of=dist/rSystemOS-v0_1.img bs=512 seek=1 count=800 conv=notrunc status=none
	dd if=dist/kernel.bin of=dist/rSystemOS-v0_1.img bs=512 seek=1 conv=notrunc status=none
	rm -f dist/*.bin dist/*.o

clean:
	rm -f dist/*.o dist/*.bin

# Also throws away the filesystem region -- the next `make all` recreates
# the image zero-filled, and fs_init() reformats it on the bad magic.
distclean: clean
	rm -f dist/rSystemOS-v0_1.img

exec:
	make all
	qemu-system-x86_64 -hda dist/rSystemOS-v0_1.img
