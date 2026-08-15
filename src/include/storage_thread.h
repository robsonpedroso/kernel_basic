#ifndef _STORAGE_THREAD_H
#define _STORAGE_THREAD_H

// Synchronous facade over a dedicated worker thread: callers block (via
// thread_block, which yields the CPU instead of spinning) while the actual
// PIO polling happens on storage_thread_main's own stack. Keeps ide.c's
// blocking read/write model but stops it from freezing every other thread
// for the whole transfer.
int storage_read_sectors(unsigned int lba, int count, void *buf);
int storage_write_sectors(unsigned int lba, int count, const void *buf);

// Runs forever; hand it to thread_create() once during boot.
void storage_thread_main(void *arg);

#endif
