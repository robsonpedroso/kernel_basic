#ifndef _DOOM_SHIM_SYS_STAT_H
#define _DOOM_SHIM_SYS_STAT_H

// m_misc.c includes this but (per a source grep) never actually calls
// stat()/uses struct stat -- this kernel's fs.c has no POSIX stat()
// equivalent to back one with anyway. mkdir() IS actually called
// (M_MakeDirectory) -- stubbed to always fail, same as fopen() for
// anything that isn't the IWAD (no writable named-file storage wired in
// yet, see doom_libc_shim.c's stdio.h comment).
int mkdir(const char *path, int mode);

#endif
