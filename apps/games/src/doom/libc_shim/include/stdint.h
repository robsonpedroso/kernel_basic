#ifndef _DOOM_SHIM_STDINT_H
#define _DOOM_SHIM_STDINT_H

// gcc's own bundled stdint.h chains to glibc's real one via #include_next
// (to merge extra glibc-specific definitions on top), which fails outright
// in a -m32 build on a host with no 32-bit glibc-dev headers installed
// (bits/libc-header-start.h missing) -- see the Phase 1.5 link-probe notes
// in apps/games/src/doom/. This shim dir is added via -I ahead of gcc's
// own system include path (see Makefile DOOM_SRCS rule), so this shadows
// both gcc's and glibc's stdint.h entirely; plain ILP32 typedefs are all
// doomgeneric/id-tech-1 source actually needs.

typedef signed char        int8_t;
typedef unsigned char       uint8_t;
typedef signed short        int16_t;
typedef unsigned short      uint16_t;
typedef signed int          int32_t;
typedef unsigned int        uint32_t;
typedef signed long long    int64_t;
typedef unsigned long long  uint64_t;

typedef int32_t  intptr_t;
typedef uint32_t uintptr_t;

#define INT8_MIN   (-128)
#define INT8_MAX   127
#define UINT8_MAX  0xFFu
#define INT16_MIN  (-32768)
#define INT16_MAX  32767
#define UINT16_MAX 0xFFFFu
#define INT32_MIN  (-2147483647-1)
#define INT32_MAX  2147483647
#define UINT32_MAX 0xFFFFFFFFu

#endif
