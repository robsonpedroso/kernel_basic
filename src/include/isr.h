#ifndef _ISR_H
#define _ISR_H

// Layout produced by isr.asm's common stubs (order matters: matches the
// push sequence of `pusha` + the manually pushed `ds`, `int_no`, `err_code`).
typedef struct registers {
	unsigned int ds;
	unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
	unsigned int int_no, err_code;
	unsigned int eip, cs, eflags, useresp, ss;
} registers_t;

typedef void (*isr_handler_t)(registers_t *regs);

void isr_handler(registers_t *regs);
void irq_handler(registers_t *regs);
void irq_install_handler(int irq, isr_handler_t handler);

// Fills IDT vectors 0-47 with the isrN/irqN stubs from isr.asm. Call after
// idt_install() (which loads an empty table) and before sti.
void isr_install(void);

#endif
