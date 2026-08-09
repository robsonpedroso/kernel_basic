#include "../include/heap.h"

// Fixed physical heap region. This kernel has no E820/memory-map probing
// yet, so instead of guessing a size-dependent offset from the end of the
// kernel image, we hardcode a region with a comfortable amount of free
// memory on every side: the kernel image (loaded at 0x1000, see
// bootloader.asm's IDE loader) has ~380KB of room before it reaches
// 0x60000; the heap then runs to 0x80000, leaving a guard gap before the
// runtime stack at 0x90000, and everything here is still well below the
// VGA framebuffer at 0xA0000 (so it's reachable even without the A20 line
// enabled).
#define HEAP_START 0x60000u
#define HEAP_SIZE  0x20000u // 128 KiB

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
	if (!heap_initialized) {
		heap_init();
	}
	if (size == 0) {
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
			return (void *)((unsigned char *)block + sizeof(block_header_t));
		}
		block = block->next;
	}

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

void kfree(void *ptr) {
	if (!ptr) {
		return;
	}
	block_header_t *block = (block_header_t *)((unsigned char *)ptr - sizeof(block_header_t));
	block->free = 1;
	coalesce();
}
