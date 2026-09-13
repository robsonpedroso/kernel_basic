#ifndef _DOOM_SHIM_ERRNO_H
#define _DOOM_SHIM_ERRNO_H

// Only a couple of edge-case fallback paths in the vendored engine check
// errno (e.g. m_misc.c's "is this actually a directory" retry) -- this
// kernel's own fs.c has no errno-style per-call error codes at all, so
// errno is set to a value that never matches the code these call sites
// compare against, letting them fall through to their other branch
// instead of behaving as if that specific condition occurred.
extern int doom_errno;
#define errno doom_errno

#define EISDIR 21

#endif
