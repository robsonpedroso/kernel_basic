#include "../include/heap.h"
#include "../include/thread.h"

// Fixed physical heap region. This kernel has no E820/memory-map probing
// yet, so instead of guessing a size-dependent offset from the end of the
// kernel image, we hardcode a region with a comfortable amount of free
// memory on every side. Moved to extended memory (past 1MiB) to make room
// for doomgeneric's working set -- its own screen buffer alone
// (320*200*sizeof(uint32_t)) is ~250KB, bigger than this heap's entire old
// 128KiB budget, before any zone/level/texture memory is even considered.
// Requires the A20 gate (see bootloader.asm) and an explicit qemu `-m`
// (see Makefile's `exec` target), since nothing here probes how much
// physical RAM actually backs this range. Runs 0x300000..0xB00000 (8MiB),
// entirely past the runtime stack at 0x200000..0x300000 (relocated there
// by bootloader.asm's init_pm once the Doom-inflated kernel image no
// longer fit below the old 0x90000 stack) -- see link.ld's ASSERT for the
// guard gap between the kernel image and that stack. HEAP_START must stay
// in lockstep with bootloader.asm's stack base/link.ld's stack-region
// comment; letting them drift apart makes kmalloc() hand out memory that
// aliases the live hardware stack.
#define HEAP_START 0x300000u
#define HEAP_SIZE  0x800000u // 8 MiB

typedef struct block_header {
	unsigned int size; // usable size, not counting this header
	int free;
	struct block_header *next;
} block_header_t;

static block_header_t *heap_head = 0;
static int heap_initialized = 0;

static void heap_init(void) {
	heap_head = (block_header_t *)HEAP_START;
	heap_head->size = HEAP_SIZE - sizeof(block_header_t);
	heap_head->free = 1;
	heap_head->next = 0;
	heap_initialized = 1;
}

void *kmalloc(unsigned int size) {
	preempt_disable();

	if (!heap_initialized) {
		heap_init();
	}
	if (size == 0) {
		preempt_enable();
		return 0;
	}

	size = (size + 3u) & ~3u; // 4-byte align

	block_header_t *block = heap_head;
	while (block) {
		if (block->free && block->size >= size) {
			// Split the block if enough room remains for another header + data.
			if (block->size >= size + sizeof(block_header_t) + 4) {
				block_header_t *rest = (block_header_t *)((unsigned char *)block + sizeof(block_header_t) + size);
				rest->size = block->size - size - sizeof(block_header_t);
				rest->free = 1;
				rest->next = block->next;

				block->size = size;
				block->next = rest;
			}
			block->free = 0;
			preempt_enable();
			return (void *)((unsigned char *)block + sizeof(block_header_t));
		}
		block = block->next;
	}

	preempt_enable();
	return 0; // out of memory
}

static void coalesce(void) {
	block_header_t *block = heap_head;
	while (block && block->next) {
		if (block->free && block->next->free) {
			block->size += sizeof(block_header_t) + block->next->size;
			block->next = block->next->next;
			continue; // re-check this block against its new neighbor
		}
		block = block->next;
	}
}

unsigned int kalloc_size(void *ptr) {
	if (!ptr) {
		return 0;
	}
	block_header_t *block = (block_header_t *)((unsigned char *)ptr - sizeof(block_header_t));
	return block->size;
}

void kfree(void *ptr) {
	if (!ptr) {
		return;
	}
	preempt_disable();
	block_header_t *block = (block_header_t *)((unsigned char *)ptr - sizeof(block_header_t));
	block->free = 1;
	coalesce();
	preempt_enable();
}
