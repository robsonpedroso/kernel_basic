#ifndef _DOOM_SHIM_SYS_TYPES_H
#define _DOOM_SHIM_SYS_TYPES_H

// m_misc.c includes this but (per a source grep) never actually uses
// mode_t/off_t/pid_t/etc from it. Empty on purpose; add real typedefs
// here if a later probe turns up an actual usage.

#endif
