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
#include "../include/thread.h"
#include "../include/storage_thread.h"

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
	ide_init();

	// thread_init() must come before fs_init(): fs.c's reads/writes now go
	// through storage_read/write_sectors, which blocks the calling thread
	// on the storage worker -- so that worker has to exist first. Neither
	// needs sti yet: thread_block()/thread_wake() are plain voluntary
	// stack switches, and ide.c's PIO polling doesn't need interrupts
	// either, so fs_init()'s first-boot auto-format can still safely run
	// before any window exists to observe a half-mounted filesystem.
	thread_init();
	thread_create("storage", storage_thread_main, 0, 4096);
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

// Owns every event coming out of event.c and everything downstream of it
// (wm.c, widgets, apps, video.c) -- the only thread that ever touches any
// of that code, which is what lets heap.c be the only place besides the
// storage queue that needs a preempt_disable/enable critical section.
static void wm_thread_main(void *arg) {
	(void)arg;
	event_st ev;
	for (;;) {
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

void main() {
	kernel_init();

	// Still thread 0, before wm_thread exists: safe to touch wm.c directly
	// this one time, and only this once.
	wm_create_window(&program_manager_app, "Program Manager", program_manager_initial_rect(),
	                  PROGRAM_MANAGER_MIN_W, PROGRAM_MANAGER_MIN_H);

	thread_create("wm", wm_thread_main, 0, 8192);

	for (;;) {
		__asm__ volatile ("hlt");
	}
}
