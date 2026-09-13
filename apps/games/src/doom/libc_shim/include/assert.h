#ifndef _DOOM_SHIM_ASSERT_H
#define _DOOM_SHIM_ASSERT_H

// No process model to abort() into a debugger from -- see doom_exit() in
// doom_libc_shim.c for what actually happens on a hard failure.
void doom_assert_fail(const char *expr, const char *file, int line);

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) ((expr) ? (void)0 : doom_assert_fail(#expr, __FILE__, __LINE__))
#endif

#endif
