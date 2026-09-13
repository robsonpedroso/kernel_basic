#ifndef _DOOM_SHIM_STDIO_H
#define _DOOM_SHIM_STDIO_H
#include <stddef.h>
#include <stdarg.h>

#ifndef NULL
#define NULL ((void *)0)
#endif
#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// Opaque -- backed by wad_disk.c's WAD blob region for reads, see
// doom_libc_shim.c. There is no real writable filesystem-of-many-files
// wired in yet (config/save-game I/O is Phase 4/5 territory per the
// project plan); fopen() for anything that isn't the IWAD just fails with
// NULL, which every call site here already treats as "no config file yet,
// use defaults" or an equally benign fallback.
typedef struct DOOM_FILE FILE;

extern FILE *doom_stdout;
extern FILE *doom_stderr;
#define stdout doom_stdout
#define stderr doom_stderr

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *f);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f);
int fseek(FILE *f, long offset, int whence);
long ftell(FILE *f);

int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int fputs(const char *s, FILE *f);
int fputc(int c, FILE *f);
int putchar(int c);
int puts(const char *s);
int fflush(FILE *f);
char *fgets(char *s, int size, FILE *f);

// Reduced: literal chars, ' ' (skips input whitespace), and %d/%i/%x/%X/%o/%u
// each taking a single `int *` -- exactly what the vendored config-parsing
// call sites (m_config.c, m_misc.c) actually use.
int sscanf(const char *str, const char *fmt, ...);

int remove(const char *path);
int rename(const char *old, const char *new_);

#endif
