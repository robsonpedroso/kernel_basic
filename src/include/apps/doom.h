#ifndef _APPS_DOOM_H
#define _APPS_DOOM_H

#include "../app.h"
#include "../wm_fwd.h"

#define DOOM_MIN_W 340
#define DOOM_MIN_H 240

extern const app_st doom_app;

// Runs the (blocking, disk-I/O) WAD-presence check once and caches the
// result for doom_on_init() to read. Must be called during boot, before
// the "wm" thread starts dispatching clicks -- see doom_app.c's own
// comment on doom_precheck_wad() for why this can't safely happen lazily
// inside doom_on_init() (it used to, and that caused a real freeze).
void doom_precheck_wad(void);

// The single live Doom window, or NULL if none is open. Written only by
// doom_app.c's on_init/on_close (main thread, inside the WM's own call
// path); read by doomgeneric_rsystemos.c's DG_DrawFrame, which runs on the
// separate kernel thread doom_app.c spawns to host doomgeneric's blocking
// D_DoomMain/D_DoomLoop (see doom_app.c). NULL after close, so a frame
// that was already in flight when the window closed just skips itself
// instead of touching the wm_window_st that wm_close_window() just
// kfree()'d -- there is no thread_kill to stop the Doom thread itself, so
// it keeps running forever in the background producing frames nobody
// reads; accepted for this phase (see doom_app.c).
wm_window_st *doom_get_active_window(void);

// Phase 3 (input). Pops one buffered (pressed, doomKey) event for
// DG_GetKey (doomgeneric_rsystemos.c) to drain -- doomgeneric's own
// polling contract is "call repeatedly until it returns 0" (see
// doomgeneric/i_input.c's I_GetEvent: `while (DG_GetKey(&pressed, &key))`),
// so this mirrors that: returns 1 and fills both outputs if an event was
// queued, 0 (outputs untouched) if the queue is empty. Producer is
// doom_app.c's on_key_down/on_key_up, which the WM only ever calls while
// the Doom window is focused (wm.c's wm_on_key_down/up both dispatch only
// to the focused window) -- so there is no separate focus check needed
// here, unlike doom_get_active_window().
int doom_dequeue_key(int *pressed, unsigned char *doomKey);

#endif
