#ifndef _DOOM_SHIM_UNISTD_H
#define _DOOM_SHIM_UNISTD_H

// i_system.c includes this but (per a source grep) never actually calls
// unlink()/usleep()/access()/etc -- timing and sleeping both go through
// doomgeneric's own DG_SleepMs/DG_GetTicksMs seam instead (see i_timer.c,
// already fully portable with zero POSIX dependency). Empty on purpose;
// add real declarations here if a later probe turns up an actual call
// site.

#endif
