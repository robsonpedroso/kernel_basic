#include "../include/timer.h"
#include "../include/isr.h"
#include "../include/event.h"
#include "../include/io.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_FREQUENCY 1193182u

static volatile unsigned int ticks = 0;

static void timer_irq_handler(registers_t *regs) {
	(void)regs;
	ticks++;
	event_push(EVENT_TIMER_TICK, (int)ticks, 0);
}

void timer_init(unsigned int frequency_hz) {
	if (frequency_hz == 0) {
		frequency_hz = 100;
	}

	unsigned int divisor = PIT_BASE_FREQUENCY / frequency_hz;

	outb(PIT_COMMAND, 0x36); // channel 0, lobyte/hibyte access, mode 3 (square wave), binary
	outb(PIT_CHANNEL0, (unsigned char)(divisor & 0xFF));
	outb(PIT_CHANNEL0, (unsigned char)((divisor >> 8) & 0xFF));

	irq_install_handler(0, timer_irq_handler);
}

unsigned int timer_get_ticks(void) {
	return ticks;
}
