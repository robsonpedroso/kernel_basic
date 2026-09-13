#ifndef _DOOM_SHIM_INTTYPES_H
#define _DOOM_SHIM_INTTYPES_H

// doomtype.h only wants this for the C99 integer types (which it says
// itself it'd rather get from stdint.h directly, see its comment) -- no
// call site in the vendored engine actually uses the PRId64-style printf
// macros this header normally also provides.
#include <stdint.h>

#endif
