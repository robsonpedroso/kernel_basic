#include "../include/event.h"

#define EVENT_QUEUE_SIZE 32

static volatile event_st queue[EVENT_QUEUE_SIZE];
static volatile int head = 0;  // next slot to write
static volatile int tail = 0;  // next slot to read
static volatile int q_count = 0;

void event_init(void) {
	head = 0;
	tail = 0;
	q_count = 0;
}

void event_push(event_type_t type, int a, int b) {
	if (q_count >= EVENT_QUEUE_SIZE) {
		return; // queue full: drop the event rather than corrupt it
	}

	queue[head].type = type;
	queue[head].a = a;
	queue[head].b = b;
	head = (head + 1) % EVENT_QUEUE_SIZE;
	q_count++;
}

void event_wait(event_st *out) {
	__asm__ volatile ("cli");

	while (q_count == 0) {
		// sti+hlt is atomic on x86: an IRQ arriving right after sti is still
		// recognized before the CPU is allowed to halt, so no wakeup is lost.
		__asm__ volatile ("sti");
		__asm__ volatile ("hlt");
		__asm__ volatile ("cli");
	}

	*out = queue[tail];
	tail = (tail + 1) % EVENT_QUEUE_SIZE;
	q_count--;

	__asm__ volatile ("sti");
}
