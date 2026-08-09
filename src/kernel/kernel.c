#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/video.h"
#include "../include/event.h"
#include "../include/gdt.h"
#include "../include/idt.h"
#include "../include/isr.h"
#include "../include/pic.h"
#include "../include/timer.h"
#include "../include/keyboard.h"
#include "../include/mouse.h"
#include "../include/serial.h"
#include "../include/gui.h"
#include "../include/rect.h"
#include "../include/wm.h"
#include "../include/cursor.h"
#include "../include/ide.h"
#include "../include/fs.h"
#include "../include/apps/program_manager.h"

static void kernel_init(void) {
	serial_init();

	// Give the kernel its own GDT before anything else -- see gdt.h for
	// why (the bootloader's original table lives in the boot sector, which
	// nothing reserves from the kernel's own .bss/.data/.heap).
	gdt_install();

	idt_install();
	isr_install();
	pic_remap();

	timer_init(100);
	keyboard_init();
	mouse_init();

	// Polled PIO, no IRQ14 -- safe to run before sti. Doing fs_init() here
	// means its first-boot auto-format happens before any window can exist
	// to observe a half-mounted filesystem.
	ide_init();
	fs_init();

	// Unmask only what we actually drive: PIT (0), keyboard (1), the
	// master->slave cascade line (2, required for any slave-PIC IRQ to
	// reach the CPU) and the mouse (12, lives on the slave PIC).
	pic_clear_mask(0);
	pic_clear_mask(1);
	pic_clear_mask(2);
	pic_clear_mask(12);

	event_init();
	cursor_init();
	wm_init();

	__asm__ volatile ("sti");

	serial_write("rSystemOS: boot ok (IDT/PIC/timer/keyboard/mouse/heap up), entering event loop\n");
}

void main() {
	kernel_init();

	wm_create_window(&program_manager_app, "Program Manager", program_manager_initial_rect(),
	                  PROGRAM_MANAGER_MIN_W, PROGRAM_MANAGER_MIN_H);

	event_st ev;
	while (1) {
		event_wait(&ev);

		switch (ev.type) {
			case EVENT_MOUSE_MOVE:
				wm_on_mouse_move(mouse_get_x(), mouse_get_y());
				break;
			case EVENT_MOUSE_DOWN:
				wm_on_mouse_down(mouse_get_x(), mouse_get_y(), ev.a);
				break;
			case EVENT_MOUSE_UP:
				wm_on_mouse_up(mouse_get_x(), mouse_get_y(), ev.a);
				break;
			case EVENT_KEY_DOWN:
				wm_on_key_down(ev.a, ev.b);
				break;
			case EVENT_TIMER_TICK:
				wm_on_tick();
				break;
			default:
				break;
		}
	}
}
