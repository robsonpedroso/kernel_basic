#ifndef _HEAP_H
#define _HEAP_H

// Simple first-fit free-list allocator over a fixed 64KiB region of
// physical memory (see heap.c for why the address is hardcoded rather
// than probed via a memory map, which this kernel doesn't have yet).
void *kmalloc(unsigned int size);
void kfree(void *ptr);

// Usable size of a live kmalloc'd block (not counting the header) -- for a
// realloc()-style caller that needs to know how much of the old block to
// preserve when growing/shrinking. 0 for a NULL ptr.
unsigned int kalloc_size(void *ptr);

#endif
