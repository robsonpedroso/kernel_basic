#include "../include/isr.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/serial.h"
#include "../include/video.h"

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

#define IDT_FLAG_INT_GATE 0x8E // present, ring0, 32-bit interrupt gate

void isr_install(void) {
	void (*isrs[32])(void) = {
		isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
		isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
		isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
		isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
	};
	void (*irqs[16])(void) = {
		irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
		irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15,
	};

	for (int i = 0; i < 32; i++) {
		idt_set_gate(i, (unsigned int)isrs[i], 0x08, IDT_FLAG_INT_GATE);
	}
	for (int i = 0; i < 16; i++) {
		idt_set_gate(32 + i, (unsigned int)irqs[i], 0x08, IDT_FLAG_INT_GATE);
	}
}

static isr_handler_t irq_routines[16] = { 0 };

void irq_install_handler(int irq, isr_handler_t handler) {
	irq_routines[irq] = handler;
}

static const char *exception_messages[32] = {
	"Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
	"Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
	"Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
	"Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
	"Coprocessor Fault", "Alignment Check", "Machine Check", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved"
};

// Self-contained hex formatter: avoids relying on lib/itoa (decimal only,
// and its buf/val argument order does not currently match its own prototype).
static void format_hex(unsigned int value, char *out) {
	const char *digits = "0123456789ABCDEF";
	out[0] = '0';
	out[1] = 'x';
	for (int i = 0; i < 8; i++) {
		out[2 + i] = digits[(value >> (28 - i * 4)) & 0xF];
	}
	out[10] = '\0';
}

void isr_handler(registers_t *regs) {
	char hex[11];

	serial_write("EXCEPTION ");
	serial_write_hex(regs->int_no);
	serial_write(" ");
	serial_write(exception_messages[regs->int_no]);
	serial_write(" err=");
	serial_write_hex(regs->err_code);
	serial_write("\n");

	kernel_print_text("\n*** KERNEL PANIC ***\n", 0);
	kernel_print_text("Exception: ", 0);
	kernel_print_text((char *)exception_messages[regs->int_no], 0);
	kernel_print_text("\nVector: ", 0);
	format_hex(regs->int_no, hex);
	kernel_print_text(hex, 0);
	kernel_print_text("  Error code: ", 0);
	format_hex(regs->err_code, hex);
	kernel_print_text(hex, 0);
	kernel_print_text("\n", 0);

	__asm__ volatile ("cli");
	for (;;) {
		__asm__ volatile ("hlt");
	}
}

void irq_handler(registers_t *regs) {
	int irq = regs->int_no - 32;

	if (irq >= 0 && irq < 16 && irq_routines[irq] != 0) {
		irq_routines[irq](regs);
	}

	pic_send_eoi((unsigned char)irq);
}
