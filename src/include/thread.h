#ifndef _THREAD_H
#define _THREAD_H

typedef enum {
	THREAD_UNUSED = 0,
	THREAD_READY,
	THREAD_RUNNING,
	THREAD_BLOCKED,
	THREAD_SLEEPING,
} thread_state_t;

typedef struct thread {
	unsigned int esp;          // valid when state != THREAD_RUNNING; context_switch's target
	unsigned char *stack_base; // kmalloc'd block; 0 for thread 0 (inherits the boot stack)
	unsigned int stack_size;
	thread_state_t state;
	unsigned int wake_tick;
	// Set by thread_wake() unconditionally, regardless of the target's
	// current state, and consumed by thread_block(): closes the race where
	// a producer enqueues work and wakes the consumer in the gap between
	// the consumer checking its queue (found nothing) and it actually
	// marking itself blocked, which would otherwise drop the wakeup.
	volatile int wake_pending;
	const char *name;
	int id;
} thread_st;

typedef void (*thread_fn_t)(void *arg);

// Registers the calling context (main()'s boot stack) as thread 0. Call once
// from kernel_init(), before any other thread_* function and before sti.
void thread_init(void);

thread_st *thread_create(const char *name, thread_fn_t entry, void *arg, unsigned int stack_size);
void thread_yield(void);
void thread_block(void);
void thread_wake(thread_st *t);
void thread_exit(void);
thread_st *thread_self(void);

// cli/sti with a nesting counter. This kernel is single-CPU, so disabling
// interrupts is a sufficient "big kernel lock" around any state a thread
// switch could otherwise observe half-updated.
void preempt_disable(void);
void preempt_enable(void);

// Called only from timer_irq_handler -- IF is already 0 there (interrupt
// gate), so it never needs preempt_disable/enable itself.
void scheduler_tick(void);

#endif
