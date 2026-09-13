#ifndef _DOOM_SHIM_STRING_H
#define _DOOM_SHIM_STRING_H
#include <stddef.h>

// Standard POSIX-shaped signatures (const-correct, size_t-based) -- this
// kernel's OWN string.h (src/include/string.h) is a separate, older,
// non-const-correct subset (int-sized, void-returning strcpy, etc.) used
// only by the GUI/kernel sources via quoted relative includes.
// doomgeneric's `#include <string.h>` (angle brackets) resolves here
// instead (this shim dir is on the -I search path for the DOOM_SRCS build
// rule only -- see Makefile), so there's no header collision -- but
// src/lib/string.c's memcpy/memset/strlen/strcmp/strcpy/strncmp are
// already linked into every build regardless (part of the kernel's own
// object list) and a second same-named definition here would be a real
// `ld` "multiple definition" error. Macro-alias exactly those to a
// doom_-prefixed implementation (see doom_libc_shim.c) that's fully
// standard-conformant (size_t, const-correct, real return values e.g.
// strcpy returning dst) rather than gambling on the kernel's simplified
// signatures being ABI-compatible with every id-tech-1 call site.

#define memcpy  doom_memcpy
#define memset  doom_memset
#define strlen  doom_strlen
#define strcmp  doom_strcmp
#define strcpy  doom_strcpy
#define strncmp doom_strncmp

void *doom_memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *doom_memset(void *dst, int c, size_t n);
int   memcmp(const void *a, const void *b, size_t n);

size_t doom_strlen(const char *s);
char *doom_strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
char *strncat(char *dst, const char *src, size_t n);
int doom_strcmp(const char *a, const char *b);
int doom_strncmp(const char *a, const char *b, size_t n);
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strdup(const char *s);
char *strerror(int errnum);

#endif
