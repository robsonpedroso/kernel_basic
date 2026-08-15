#include "../include/thread.h"
#include "../include/heap.h"

#define MAX_THREADS 8
#define THREAD_QUANTUM_TICKS 2

extern void context_switch(unsigned int *old_esp_store, unsigned int new_esp);
extern void thread_trampoline(void);

static thread_st threads[MAX_THREADS];
static thread_st *current = 0;
static int next_id = 0;
static volatile int disable_count = 0;
static unsigned int quantum_remaining = THREAD_QUANTUM_TICKS;

void preempt_disable(void) {
	__asm__ volatile ("cli");
	disable_count++;
}

void preempt_enable(void) {
	if (disable_count > 0) {
		disable_count--;
	}
	if (disable_count == 0) {
		__asm__ volatile ("sti");
	}
}

void thread_init(void) {
	threads[0].state = THREAD_RUNNING;
	threads[0].stack_base = 0;
	threads[0].stack_size = 0;
	threads[0].wake_pending = 0;
	threads[0].name = "main";
	threads[0].id = next_id++;
	current = &threads[0];
}

static thread_st *find_free_slot(void) {
	for (int i = 0; i < MAX_THREADS; i++) {
		if (threads[i].state == THREAD_UNUSED) {
			return &threads[i];
		}
	}
	return 0;
}

thread_st *thread_create(const char *name, thread_fn_t entry, void *arg, unsigned int stack_size) {
	thread_st *t = find_free_slot();
	if (!t) {
		return 0;
	}

	unsigned char *stack = (unsigned char *)kmalloc(stack_size);
	if (!stack) {
		return 0;
	}

	// Stack grows down: fabricate the frame context_switch() pops the very
	// first time this thread is scheduled (see context_switch.asm). Layout,
	// low to high address: edi(unused), esi(=entry), ebx(=arg), ebp(0),
	// return address (thread_trampoline).
	unsigned int *sp = (unsigned int *)(stack + stack_size);
	*(--sp) = (unsigned int)thread_trampoline;
	*(--sp) = 0;
	*(--sp) = (unsigned int)arg;
	*(--sp) = (unsigned int)entry;
	*(--sp) = 0;

	t->esp = (unsigned int)sp;
	t->stack_base = stack;
	t->stack_size = stack_size;
	t->wake_pending = 0;
	t->state = THREAD_READY;
	t->name = name;
	t->id = next_id++;
	return t;
}

// Scans every slot but the current thread, round-robin from wherever it
// left off. Falls back to `current` only if it is still runnable (the
// "nobody else is ready, keep going" case for scheduler_tick/thread_yield);
// returns 0 if truly nothing is runnable, which thread_block treats as "do
// not actually block" since this kernel always keeps thread 0 idling.
static thread_st *pick_next(void) {
	int start = (int)(current - threads);
	for (int i = 1; i <= MAX_THREADS; i++) {
		thread_st *t = &threads[(start + i) % MAX_THREADS];
		if (t->state == THREAD_READY) {
			return t;
		}
	}
	return (current->state == THREAD_RUNNING) ? current : 0;
}

static void switch_to(thread_st *next) {
	thread_st *prev = current;
	if (prev->state == THREAD_RUNNING) {
		prev->state = THREAD_READY;
	}
	next->state = THREAD_RUNNING;
	current = next;
	context_switch(&prev->esp, next->esp);
}

void scheduler_tick(void) {
	if (quantum_remaining > 0) {
		quantum_remaining--;
		return;
	}
	quantum_remaining = THREAD_QUANTUM_TICKS;

	thread_st *next = pick_next();
	if (next && next != current) {
		switch_to(next);
	}
}

// thread_yield/thread_block deliberately use a bare cli/sti, not
// preempt_disable/enable's shared nesting counter: they call switch_to(),
// and switch_to() can hand the CPU to a thread that is itself mid-way
// through one of these same two functions. That thread will run its own
// trailing sti unconditionally once it resumes -- correct on its own, but
// if getting there had also meant incrementing OUR counter, the increment
// from the thread that switched away is left with nobody left to match it
// (nothing "returns" to run its enable), permanently wedging the counter
// above 0 and disabling interrupts for good the next time anything expects
// sti at count 0 (kmalloc's next call, for instance). A plain cli/sti pair
// scoped to a single call has no such cross-thread bookkeeping to leak:
// whichever thread's own call this is, resuming it runs exactly its own
// sti, once. kmalloc/kfree and the storage queue's enqueue/dequeue are the
// ones that actually need preempt_disable/enable's nesting counter, and
// they're safe with it precisely because they never call switch_to while
// holding it.
void thread_yield(void) {
	__asm__ volatile ("cli");
	thread_st *next = pick_next();
	if (next && next != current) {
		switch_to(next);
	}
	__asm__ volatile ("sti");
}

void thread_block(void) {
	__asm__ volatile ("cli");

	// A wake already arrived before we got here (racing with whatever made
	// us decide to block, e.g. a producer enqueueing work right after we
	// checked an empty queue) -- consume it instead of blocking, or the
	// wakeup would be lost with nothing left to deliver it to.
	if (current->wake_pending) {
		current->wake_pending = 0;
		__asm__ volatile ("sti");
		return;
	}

	current->state = THREAD_BLOCKED;
	thread_st *next = pick_next();
	if (next) {
		switch_to(next);
	} else {
		current->state = THREAD_RUNNING;
	}
	current->wake_pending = 0;
	__asm__ volatile ("sti");
}

void thread_wake(thread_st *t) {
	preempt_disable();
	t->wake_pending = 1;
	if (t->state == THREAD_BLOCKED || t->state == THREAD_SLEEPING) {
		t->state = THREAD_READY;
	}
	preempt_enable();
}

void thread_exit(void) {
	// Deliberately a bare cli, not preempt_disable(): this call never
	// returns, so it can never reach a matching preempt_enable() itself --
	// going through the shared nesting counter here would leak one
	// increment forever and permanently stop some future preempt_enable()
	// from ever reaching 0 (i.e. interrupts would never turn back on).
	__asm__ volatile ("cli");
	current->state = THREAD_UNUSED;
	// stack_base is intentionally leaked: freeing the stack this thread is
	// still executing on, right before switching off of it for good, would
	// hand kfree's coalesce() a live block to merge with.
	thread_st *next = pick_next();
	if (!next) {
		__asm__ volatile ("sti");
		for (;;) { __asm__ volatile ("hlt"); }
	}
	switch_to(next);
	for (;;) { } // unreachable
}

thread_st *thread_self(void) {
	return current;
}
