#ifndef _APP_H
#define _APP_H

#include "rect.h"
#include "wm_fwd.h"
#include "icon.h"

// Contract every windowed application implements. The WM never knows
// anything about a specific app beyond this table (and comparing the
// pointer to find an already-open singleton instance).
typedef struct app {
	const char *name;

	// ICON_NONE (0) by default via designated initializers -- icon_draw()
	// falls back to the bevel-box placeholder for any app that doesn't set
	// this explicitly.
	icon_id_t icon;

	// Required: allocate/initialize app-private state, returned opaquely
	// and passed back into every other callback below.
	void *(*on_init)(wm_window_st *win);

	// Required: full redraw of the content area (window chrome is already
	// drawn by the caller).
	void (*on_show)(wm_window_st *win, void *state, rect_st content);

	void (*on_key_down)(wm_window_st *win, void *state, int ascii, int mods);

	// lx/ly are content-relative (0,0 = top-left of the content rect).
	void (*on_mouse_down)(wm_window_st *win, void *state, int lx, int ly, int buttons);
	void (*on_mouse_up)(wm_window_st *win, void *state, int lx, int ly, int buttons);
	void (*on_mouse_move)(wm_window_st *win, void *state, int lx, int ly);

	// Optional: called every timer tick while the window isn't minimized.
	int (*on_tick)(wm_window_st *win, void *state);

	// Optional: release anything on_init allocated (kfree etc.) before the
	// WM frees the window itself.
	void (*on_close)(wm_window_st *win, void *state);

	// Optional: called when a DIFFERENT window becomes focused (wm.c's
	// wm_raise/wm_minimize_window), never on close. Every existing app
	// leaves this NULL via designated initializers, so adding it here
	// doesn't touch them. Added for menubar_st: an open dropdown has no
	// other way to notice the user clicked a sibling window's title bar,
	// since HIT_TITLE never reaches on_mouse_down.
	void (*on_blur)(wm_window_st *win, void *state);
} app_st;

#endif
