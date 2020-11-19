all:
	nasm -f bin src/boot/bootloader.asm -o dist/bootloader.bin
	gcc -fno-pie -c src/kernel/kernel.c -o dist/kernel.o -m32
	gcc -fno-pie -c src/lib/stdio.c -o dist/stdio.o -m32
	gcc -fno-pie -c src/lib/stdlib.c -o dist/stdlib.o -m32
	gcc -fno-pie -c src/lib/string.c -o dist/string.o -m32
	gcc -fno-pie -c src/drivers/video.c -o dist/video.o -m32
	gcc -fno-pie -c src/drivers/keyboard.c -o dist/keyboard.o -m32
	nasm -f elf32 src/kernel/load_kernel.asm -o dist/load_kernel.o 
	ld -melf_i386 -o dist/kernel.bin -Ttext 0x1000 dist/load_kernel.o dist/kernel.o dist/stdlib.o dist/stdio.o dist/string.o dist/video.o dist/keyboard.o --oformat binary -T link.ld
	cat dist/bootloader.bin dist/kernel.bin > dist/rSystemOS-v0_1.img
	rm dist/*.bin dist/*.o

exec:
	make all
	qemu-system-x86_64 dist/rSystemOS-v0_1.img