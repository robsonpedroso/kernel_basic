#include "../include/storage_thread.h"
#include "../include/ide.h"
#include "../include/thread.h"

typedef enum { IO_READ, IO_WRITE } io_op_t;

typedef struct io_request {
	io_op_t op;
	unsigned int lba;
	int count;
	void *buf;
	int result;
	volatile int done;
	thread_st *waiter;
	struct io_request *next;
} io_request_t;

static io_request_t *queue_head = 0;
static io_request_t *queue_tail = 0;
static thread_st *worker = 0;

static void enqueue(io_request_t *req) {
	preempt_disable();
	req->next = 0;
	if (queue_tail) {
		queue_tail->next = req;
	} else {
		queue_head = req;
	}
	queue_tail = req;
	preempt_enable();

	// worker is still 0 if storage_thread_main hasn't run for the first
	// time yet -- harmless: the request is already queued, so its first
	// loop iteration picks this up regardless of whether anyone "woke" it.
	if (worker) {
		thread_wake(worker);
	}
}

static io_request_t *dequeue(void) {
	preempt_disable();
	io_request_t *req = queue_head;
	if (req) {
		queue_head = req->next;
		if (!queue_head) {
			queue_tail = 0;
		}
	}
	preempt_enable();
	return req;
}

static int submit(io_op_t op, unsigned int lba, int count, void *buf) {
	io_request_t req;
	req.op = op;
	req.lba = lba;
	req.count = count;
	req.buf = buf;
	req.result = -1;
	req.done = 0;
	req.waiter = thread_self();
	req.next = 0;

	enqueue(&req);
	while (!req.done) {
		thread_block();
	}
	return req.result;
}

int storage_read_sectors(unsigned int lba, int count, void *buf) {
	return submit(IO_READ, lba, count, buf);
}

int storage_write_sectors(unsigned int lba, int count, const void *buf) {
	return submit(IO_WRITE, lba, count, (void *)buf);
}

void storage_thread_main(void *arg) {
	(void)arg;
	worker = thread_self();
	for (;;) {
		io_request_t *req = dequeue();
		if (!req) {
			thread_block();
			continue;
		}

		if (req->op == IO_READ) {
			req->result = ide_read_sectors(req->lba, req->count, req->buf);
		} else {
			req->result = ide_write_sectors(req->lba, req->count, req->buf);
		}
		req->done = 1;
		thread_wake(req->waiter);
	}
}
