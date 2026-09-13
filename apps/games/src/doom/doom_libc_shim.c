// Minimal freestanding libc for the vendored doomgeneric engine (see
// apps/games/src/doom/libc_shim/include/ for the header side of this).
// Scoped to exactly what the Phase 1.5 link-probe + a static grep survey
// of the vendored source turned up as actually used -- not a general
// libc. Backed by this kernel's own primitives: kmalloc/kfree (heap.h),
// wad_read_range (wad_disk.h) for the one file doomgeneric can open (the
// IWAD), and serial_write (serial.h) as the only "console" available.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>

#include "../../../../src/include/heap.h"
#include "../../../../src/include/wad_disk.h"
#include "../../../../src/include/serial.h"

// ---- errno / assert ----

int doom_errno = 0;

void doom_assert_fail(const char *expr, const char *file, int line) {
	(void)file;
	(void)line;
	serial_write("rSystemOS: doom assert failed: ");
	serial_write(expr);
	serial_write("\n");
	for (;;) {
		__asm__ volatile ("hlt");
	}
}

// ---- string.h (doom_-prefixed to avoid colliding with src/lib/string.c's
// own memcpy/memset/strlen/strcmp/strcpy/strncmp, already linked into
// every build regardless -- see the shim string.h header comment) ----

void *doom_memcpy(void *dst, const void *src, size_t n) {
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	for (size_t i = 0; i < n; i++) {
		d[i] = s[i];
	}
	return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	if (d < s) {
		for (size_t i = 0; i < n; i++) {
			d[i] = s[i];
		}
	} else if (d > s) {
		for (size_t i = n; i > 0; i--) {
			d[i - 1] = s[i - 1];
		}
	}
	return dst;
}

void *doom_memset(void *dst, int c, size_t n) {
	unsigned char *d = (unsigned char *)dst;
	for (size_t i = 0; i < n; i++) {
		d[i] = (unsigned char)c;
	}
	return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
	const unsigned char *pa = (const unsigned char *)a;
	const unsigned char *pb = (const unsigned char *)b;
	for (size_t i = 0; i < n; i++) {
		if (pa[i] != pb[i]) {
			return (int)pa[i] - (int)pb[i];
		}
	}
	return 0;
}

size_t doom_strlen(const char *s) {
	size_t n = 0;
	while (s[n]) {
		n++;
	}
	return n;
}

char *doom_strcpy(char *dst, const char *src) {
	char *ret = dst;
	while ((*dst++ = *src++)) {
	}
	return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
	size_t i = 0;
	for (; i < n && src[i]; i++) {
		dst[i] = src[i];
	}
	for (; i < n; i++) {
		dst[i] = '\0';
	}
	return dst;
}

char *strcat(char *dst, const char *src) {
	char *ret = dst;
	while (*dst) {
		dst++;
	}
	while ((*dst++ = *src++)) {
	}
	return ret;
}

char *strncat(char *dst, const char *src, size_t n) {
	char *ret = dst;
	while (*dst) {
		dst++;
	}
	size_t i = 0;
	for (; i < n && src[i]; i++) {
		dst[i] = src[i];
	}
	dst[i] = '\0';
	return ret;
}

int doom_strcmp(const char *a, const char *b) {
	while (*a && (*a == *b)) {
		a++;
		b++;
	}
	return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int doom_strncmp(const char *a, const char *b, size_t n) {
	for (size_t i = 0; i < n; i++) {
		if (a[i] != b[i] || a[i] == '\0') {
			return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
		}
	}
	return 0;
}

int strcasecmp(const char *a, const char *b) {
	while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
		a++;
		b++;
	}
	return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

int strncasecmp(const char *a, const char *b, size_t n) {
	for (size_t i = 0; i < n; i++) {
		int ca = tolower((unsigned char)a[i]);
		int cb = tolower((unsigned char)b[i]);
		if (ca != cb || a[i] == '\0') {
			return ca - cb;
		}
	}
	return 0;
}

char *strchr(const char *s, int c) {
	while (*s) {
		if (*s == (char)c) {
			return (char *)s;
		}
		s++;
	}
	return (c == '\0') ? (char *)s : 0;
}

char *strrchr(const char *s, int c) {
	const char *found = 0;
	while (*s) {
		if (*s == (char)c) {
			found = s;
		}
		s++;
	}
	if (c == '\0') {
		return (char *)s;
	}
	return (char *)found;
}

char *strstr(const char *haystack, const char *needle) {
	if (!*needle) {
		return (char *)haystack;
	}
	for (; *haystack; haystack++) {
		const char *h = haystack, *n = needle;
		while (*h && *n && *h == *n) {
			h++;
			n++;
		}
		if (!*n) {
			return (char *)haystack;
		}
	}
	return 0;
}

char *strdup(const char *s) {
	size_t len = doom_strlen(s) + 1;
	char *p = (char *)kmalloc((unsigned int)len);
	if (p) {
		doom_memcpy(p, s, len);
	}
	return p;
}

char *strerror(int errnum) {
	(void)errnum; // no per-code message table -- nothing here inspects the string itself
	return "error";
}

// ---- ctype.h ----

int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c; }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isprint(int c) { return c >= 0x20 && c < 0x7F; }
int iscntrl(int c) { return (c >= 0 && c < 0x20) || c == 0x7F; }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int ispunct(int c) { return isprint(c) && c != ' ' && !isalnum(c); }

// ---- stdlib.h: heap ----

void *malloc(size_t size) { return kmalloc((unsigned int)size); }
void free(void *ptr) { kfree(ptr); }

void *calloc(size_t nmemb, size_t size) {
	unsigned int total = (unsigned int)(nmemb * size);
	void *p = kmalloc(total);
	if (p) {
		doom_memset(p, 0, total);
	}
	return p;
}

void *realloc(void *ptr, size_t size) {
	if (!ptr) {
		return kmalloc((unsigned int)size);
	}
	if (size == 0) {
		kfree(ptr);
		return 0;
	}
	unsigned int old_size = kalloc_size(ptr);
	void *p = kmalloc((unsigned int)size);
	if (!p) {
		return 0;
	}
	unsigned int copy = (old_size < (unsigned int)size) ? old_size : (unsigned int)size;
	doom_memcpy(p, ptr, copy);
	kfree(ptr);
	return p;
}

int abs(int n) { return n < 0 ? -n : n; }
long labs(long n) { return n < 0 ? -n : n; }

int doom_atoi(const char *s) {
	int sign = 1, val = 0;
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	if (*s == '-') {
		sign = -1;
		s++;
	} else if (*s == '+') {
		s++;
	}
	while (*s >= '0' && *s <= '9') {
		val = val * 10 + (*s - '0');
		s++;
	}
	return val * sign;
}

double atof(const char *s) {
	// Minimal: sign + integer + fractional digits, no exponent -- the
	// vendored config/argument parsing this feeds never emits scientific
	// notation.
	double sign = 1.0, val = 0.0, frac = 0.0, scale = 0.1;
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	if (*s == '-') {
		sign = -1.0;
		s++;
	} else if (*s == '+') {
		s++;
	}
	while (*s >= '0' && *s <= '9') {
		val = val * 10.0 + (double)(*s - '0');
		s++;
	}
	if (*s == '.') {
		s++;
		while (*s >= '0' && *s <= '9') {
			frac += (double)(*s - '0') * scale;
			scale *= 0.1;
			s++;
		}
	}
	return sign * (val + frac);
}

long strtol(const char *s, char **endptr, int base) {
	(void)base; // only base-10 needed by any call site here
	long sign = 1, val = 0;
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	if (*s == '-') {
		sign = -1;
		s++;
	} else if (*s == '+') {
		s++;
	}
	while (*s >= '0' && *s <= '9') {
		val = val * 10 + (*s - '0');
		s++;
	}
	if (endptr) {
		*endptr = (char *)s;
	}
	return val * sign;
}

// No process model to exit a process into -- halt the CPU (matches
// kernel.c's own idle-loop pattern) after logging to serial, rather than
// returning to a caller that assumes it never comes back.
void exit(int code) {
	serial_write("rSystemOS: doom exit(");
	serial_write_hex((unsigned int)code);
	serial_write(") -- halting\n");
	for (;;) {
		__asm__ volatile ("hlt");
	}
}

void abort(void) {
	serial_write("rSystemOS: doom abort()\n");
	exit(-1);
}

char *getenv(const char *name) {
	(void)name;
	return 0;
}

int system(const char *cmd) {
	(void)cmd;
	return -1;
}

static unsigned int g_rand_state = 12345u;

int rand(void) {
	g_rand_state = g_rand_state * 1103515245u + 12345u;
	return (int)((g_rand_state >> 16) & 0x7FFFu);
}

void srand(unsigned int seed) {
	g_rand_state = seed;
}

void qsort(void *base, size_t nmemb, size_t size, int (*cmp)(const void *, const void *)) {
	// Insertion sort: O(n^2), but every call site here sorts small,
	// per-frame arrays (visplanes, sprites) -- see the project plan's own
	// reasoning for why a "real" quicksort isn't worth the code here.
	unsigned char *arr = (unsigned char *)base;
	unsigned char tmp[64];
	unsigned int sz = (unsigned int)size;
	if (sz > sizeof(tmp)) {
		sz = sizeof(tmp); // not expected to hit -- defensive only
	}
	for (size_t i = 1; i < nmemb; i++) {
		size_t j = i;
		while (j > 0 && cmp(arr + j * size, arr + (j - 1) * size) < 0) {
			doom_memcpy(tmp, arr + j * size, sz);
			doom_memcpy(arr + j * size, arr + (j - 1) * size, sz);
			doom_memcpy(arr + (j - 1) * size, tmp, sz);
			j--;
		}
	}
}

// ---- stdio.h ----
//
// Only the IWAD is backed by real storage (see wad_disk.h) -- fopen()
// for anything else fails with NULL, which every call site here already
// treats as a benign "no config/save file yet, use defaults" case (this
// port has no writable named-file storage wired in yet; see the project
// plan's Phase 4/5 notes). stdout/stderr both route to the serial port,
// the only "console" this kernel has.

struct DOOM_FILE {
	unsigned int pos;
	unsigned int size;
	int valid;
};

static struct DOOM_FILE g_wad_file;
static struct DOOM_FILE g_stdout_marker;
static struct DOOM_FILE g_stderr_marker;

FILE *doom_stdout = &g_stdout_marker;
FILE *doom_stderr = &g_stderr_marker;

static int has_suffix(const char *s, const char *suffix) {
	size_t ls = doom_strlen(s), lf = doom_strlen(suffix);
	if (lf > ls) {
		return 0;
	}
	for (size_t i = 0; i < lf; i++) {
		if (tolower((unsigned char)s[ls - lf + i]) != tolower((unsigned char)suffix[i])) {
			return 0;
		}
	}
	return 1;
}

FILE *fopen(const char *path, const char *mode) {
	(void)mode;
	if (!has_suffix(path, ".wad") || g_wad_file.valid) {
		return 0;
	}
	g_wad_file.pos = 0;
	// True size unknown at runtime (no host-stat data embedded in the
	// image yet -- see wad_disk.h) -- reports the whole reserved region.
	// Harmless for the offset-based wad_file_class_t reads w_file_stdc.c
	// actually uses; M_FileLength()-based whole-file preloads would see
	// this padding too, but nothing on the IWAD path does that.
	g_wad_file.size = WAD_MAX_SECTORS * 512u;
	g_wad_file.valid = 1;
	return &g_wad_file;
}

int fclose(FILE *f) {
	if (f == &g_wad_file) {
		f->valid = 0;
	}
	return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *f) {
	if (!f || f == doom_stdout || f == doom_stderr) {
		return 0;
	}
	unsigned int want = (unsigned int)(size * nmemb);
	if (f->pos + want > f->size) {
		want = (f->size > f->pos) ? (f->size - f->pos) : 0;
	}
	if (want == 0 || wad_read_range(f->pos, ptr, want) != 0) {
		return 0;
	}
	f->pos += want;
	return (size > 0) ? (want / size) : 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f) {
	(void)ptr;
	(void)size;
	(void)nmemb;
	(void)f;
	return 0; // read-only WAD backing; nothing else writable yet
}

int fseek(FILE *f, long offset, int whence) {
	if (!f || f == doom_stdout || f == doom_stderr) {
		return -1;
	}
	long base = (whence == SEEK_SET) ? 0 : (whence == SEEK_CUR) ? (long)f->pos : (long)f->size;
	long np = base + offset;
	if (np < 0) {
		return -1;
	}
	f->pos = (unsigned int)np;
	return 0;
}

long ftell(FILE *f) {
	if (!f) {
		return -1;
	}
	return (long)f->pos;
}

int remove(const char *path) {
	(void)path;
	return -1;
}

int rename(const char *old, const char *new_) {
	(void)old;
	(void)new_;
	return -1;
}

// ---- printf family: reduced formatter (%d %i %u %x %X %o %c %s %p %%,
// width + zero-pad flag, l/h length modifiers consumed-not-honored) --
// covers what the static survey of the vendored source actually found. ----

static void out_char(char *buf, unsigned int size, unsigned int *pos, char c) {
	if (*pos + 1 < size) {
		buf[*pos] = c;
	}
	(*pos)++;
}

static void out_str(char *buf, unsigned int size, unsigned int *pos, const char *s) {
	while (*s) {
		out_char(buf, size, pos, *s++);
	}
}

static void out_uint(char *buf, unsigned int size, unsigned int *pos, unsigned int val, int base, int upper, int width, int zero_pad) {
	char tmp[32];
	int n = 0;
	const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	if (val == 0) {
		tmp[n++] = '0';
	}
	while (val) {
		tmp[n++] = digits[val % (unsigned int)base];
		val /= (unsigned int)base;
	}
	int pad = width - n;
	while (pad-- > 0) {
		out_char(buf, size, pos, zero_pad ? '0' : ' ');
	}
	while (n > 0) {
		out_char(buf, size, pos, tmp[--n]);
	}
}

static void out_int(char *buf, unsigned int size, unsigned int *pos, int val, int width, int zero_pad) {
	unsigned int uval;
	if (val < 0) {
		out_char(buf, size, pos, '-');
		uval = (unsigned int)(-val);
		if (width > 0) {
			width--;
		}
	} else {
		uval = (unsigned int)val;
	}
	out_uint(buf, size, pos, uval, 10, 0, width, zero_pad);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
	unsigned int usize = (unsigned int)size;
	unsigned int pos = 0;
	while (*fmt) {
		if (*fmt != '%') {
			out_char(buf, usize, &pos, *fmt++);
			continue;
		}
		fmt++;
		int zero_pad = 0, width = 0;
		while (*fmt == '0' || *fmt == '-') {
			if (*fmt == '0') {
				zero_pad = 1;
			}
			fmt++;
		}
		while (*fmt >= '0' && *fmt <= '9') {
			width = width * 10 + (*fmt - '0');
			fmt++;
		}
		while (*fmt == 'l' || *fmt == 'h') {
			fmt++;
		}

		switch (*fmt) {
			case 'd':
			case 'i':
				out_int(buf, usize, &pos, va_arg(ap, int), width, zero_pad);
				break;
			case 'u':
				out_uint(buf, usize, &pos, va_arg(ap, unsigned int), 10, 0, width, zero_pad);
				break;
			case 'x':
				out_uint(buf, usize, &pos, va_arg(ap, unsigned int), 16, 0, width, zero_pad);
				break;
			case 'X':
				out_uint(buf, usize, &pos, va_arg(ap, unsigned int), 16, 1, width, zero_pad);
				break;
			case 'o':
				out_uint(buf, usize, &pos, va_arg(ap, unsigned int), 8, 0, width, zero_pad);
				break;
			case 'p':
				out_str(buf, usize, &pos, "0x");
				out_uint(buf, usize, &pos, (unsigned int)(uintptr_t)va_arg(ap, void *), 16, 0, 8, 1);
				break;
			case 'c':
				out_char(buf, usize, &pos, (char)va_arg(ap, int));
				break;
			case 's': {
				const char *s = va_arg(ap, const char *);
				out_str(buf, usize, &pos, s ? s : "(null)");
				break;
			}
			case '%':
				out_char(buf, usize, &pos, '%');
				break;
			case '\0':
				goto done;
			default:
				out_char(buf, usize, &pos, '%');
				out_char(buf, usize, &pos, *fmt);
				break;
		}
		fmt++;
	}
done:
	if (usize > 0) {
		buf[(pos < usize) ? pos : usize - 1] = '\0';
	}
	return (int)pos;
}

int vfprintf(FILE *f, const char *fmt, va_list ap) {
	(void)f; // both stdout and stderr route to serial -- see doom_stdout/doom_stderr
	char tmp[256];
	int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
	serial_write(tmp);
	return n;
}

int fprintf(FILE *f, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int n = vfprintf(f, fmt, ap);
	va_end(ap);
	return n;
}

int printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int n = vfprintf(doom_stdout, fmt, ap);
	va_end(ap);
	return n;
}

int sprintf(char *buf, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, 0x7FFFFFFFu, fmt, ap); // sprintf has no length limit by contract
	va_end(ap);
	return n;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return n;
}

int fputs(const char *s, FILE *f) {
	(void)f;
	serial_write(s);
	return 0;
}

int fputc(int c, FILE *f) {
	(void)f;
	serial_write_char((char)c);
	return c;
}

int putchar(int c) {
	serial_write_char((char)c);
	return c;
}

char *fgets(char *s, int size, FILE *f) {
	(void)s;
	(void)size;
	(void)f;
	return 0; // no interactive/config stdin -- callers already treat NULL as EOF
}

int puts(const char *s) {
	serial_write(s);
	serial_write_char('\n');
	return 0;
}

int fflush(FILE *f) {
	(void)f;
	return 0;
}

int mkdir(const char *path, int mode) {
	(void)path;
	(void)mode;
	return -1; // no writable named-file storage wired in yet
}

int sscanf(const char *str, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int matched = 0;
	while (*fmt) {
		if (*fmt == ' ') {
			while (*str == ' ' || *str == '\t') {
				str++;
			}
			fmt++;
			continue;
		}
		if (*fmt == '%') {
			fmt++;
			while (*fmt >= '0' && *fmt <= '9') {
				fmt++; // width, ignored -- no call site here uses it
			}
			char spec = *fmt++;
			while (*str == ' ' || *str == '\t') {
				str++; // numeric conversions skip leading input whitespace too
			}
			int base = 10;
			if (spec == 'x' || spec == 'X') {
				base = 16;
			} else if (spec == 'o') {
				base = 8;
			} else if (spec == 'i') {
				if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
					base = 16;
					str += 2;
				} else if (str[0] == '0' && str[1]) {
					base = 8;
					str += 1;
				}
			}
			int neg = 0;
			if (*str == '-') {
				neg = 1;
				str++;
			} else if (*str == '+') {
				str++;
			}
			int val = 0, any = 0;
			for (;;) {
				char c = *str;
				int d;
				if (c >= '0' && c <= '9') {
					d = c - '0';
				} else if (c >= 'a' && c <= 'f') {
					d = c - 'a' + 10;
				} else if (c >= 'A' && c <= 'F') {
					d = c - 'A' + 10;
				} else {
					break;
				}
				if (d >= base) {
					break;
				}
				val = val * base + d;
				str++;
				any = 1;
			}
			if (!any) {
				break; // conversion failed -- stop, matching real sscanf
			}
			int *out = va_arg(ap, int *);
			*out = neg ? -val : val;
			matched++;
			continue;
		}
		if (*str != *fmt) {
			break;
		}
		str++;
		fmt++;
	}
	va_end(ap);
	return matched;
}

// ---- math.h: x87 FPU instructions directly (no libm, no libgcc linked --
// see link line in Makefile). Only R_InitTextureMapping's one-time setup
// (sin/tan) and a mouse-acceleration check this port never reaches at
// runtime (fabs, usemouse is always 0) touch this at all. kernel_init()
// runs one `fninit` at boot so the FPU starts in a known-good state. ----

double sin(double x) {
	double result;
	__asm__ volatile ("fsin" : "=t"(result) : "0"(x));
	return result;
}

double cos(double x) {
	double result;
	__asm__ volatile ("fcos" : "=t"(result) : "0"(x));
	return result;
}

double tan(double x) {
	double result;
	__asm__ volatile ("fptan" : "=t"(result) : "0"(x));
	__asm__ volatile ("fstp %%st(0)" ::: "st");
	return result;
}

double fabs(double x) {
	return x < 0 ? -x : x;
}

// ---- 64-bit signed division ----
//
// m_fixed.c's FixedDiv2 does `((int64_t)a << FRACBITS) / b` -- a 64-bit /
// 64-bit division gcc lowers to a call to this libgcc helper on i386
// rather than an inline instruction (there's no native 64/64 divide on
// this target). Normally satisfied by linking libgcc.a, but this
// project's final link is a raw `ld --oformat binary` (see Makefile),
// with no -lgcc anywhere -- so it needs its own definition here instead.
// Plain schoolbook shift-subtract: correctness over speed, since
// FixedDiv's actual runtime values never come close to needing all 64
// bits. Revisit with a hardware-idiv fast path (divisor always fits in
// 32 bits for every real fixed-point value in this engine) if profiling
// ever shows this as hot.
long long __divdi3(long long a, long long b) {
	int neg = 0;
	unsigned long long ua, ub;

	if (a < 0) {
		neg ^= 1;
		ua = (unsigned long long)(-a);
	} else {
		ua = (unsigned long long)a;
	}
	if (b < 0) {
		neg ^= 1;
		ub = (unsigned long long)(-b);
	} else {
		ub = (unsigned long long)b;
	}

	if (ub == 0) {
		return 0; // avoid UB on divide-by-zero; real hardware would trap
	}

	unsigned long long quotient = 0, remainder = 0;
	for (int i = 63; i >= 0; i--) {
		remainder = (remainder << 1) | ((ua >> i) & 1ULL);
		if (remainder >= ub) {
			remainder -= ub;
			quotient |= (1ULL << i);
		}
	}

	return neg ? -(long long)quotient : (long long)quotient;
}
