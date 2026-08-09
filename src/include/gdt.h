#ifndef _GDT_H
#define _GDT_H

// The kernel runs with the GDT bootloader.asm built at 0x7C00-0x7DFF (the
// boot sector itself) purely because that's what was loaded when the CPU
// switched to protected mode -- nothing ever relocated it. Nothing reserves
// that address range from the kernel's own .bss/.data/.heap, so as the
// kernel grows, a large-enough static array (this has already happened:
// idt.c's 2KB `idt[256]` table) can land on top of it. Reads/writes to CS
// and friends don't notice immediately (the CPU caches the loaded
// descriptor), but the very next interrupt -- which reloads CS from the
// IDT gate's selector via the GDT -- takes a #GP and the kernel triple
// faults. gdt_install() gives the kernel its own GDT in normal, permanent
// kernel memory so this class of bug can't recur as the kernel keeps
// growing. Selectors are unchanged (0x08 code / 0x10 data) so nothing
// elsewhere (isr.asm, isr_install) needs to change.
void gdt_install(void);

#endif
