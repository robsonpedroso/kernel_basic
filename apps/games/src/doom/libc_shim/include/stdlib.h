#ifndef _DOOM_SHIM_STDLIB_H
#define _DOOM_SHIM_STDLIB_H
#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

int abs(int n);
long labs(long n);

// atoi collides with src/lib/stdlib.c's own atoi (already linked into
// every build) -- see string.h's comment on the same pattern.
#define atoi doom_atoi
int doom_atoi(const char *s);
double atof(const char *s);
long strtol(const char *s, char **endptr, int base);

// No process model to exit a process into -- see doom_exit() in
// doom_libc_shim.c (halts the CPU after logging to serial).
void exit(int code);
void abort(void);

char *getenv(const char *name);
int system(const char *cmd);

void qsort(void *base, size_t nmemb, size_t size,
           int (*cmp)(const void *, const void *));

#define RAND_MAX 0x7FFFFFFF
int rand(void);
void srand(unsigned int seed);

#endif
