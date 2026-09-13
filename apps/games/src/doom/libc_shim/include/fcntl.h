#ifndef _DOOM_SHIM_FCNTL_H
#define _DOOM_SHIM_FCNTL_H

// i_input.c/i_video.c include this but (per a source grep) never actually
// call open()/use O_* flags -- this port only ever "opens" the IWAD, via
// fopen (see doom_libc_shim.c). Empty on purpose; add real declarations
// here if a later probe turns up an actual call site.

#endif
