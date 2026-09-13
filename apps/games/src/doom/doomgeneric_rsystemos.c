#include "doomgeneric/doomgeneric.h"
#include "doomgeneric/doomkeys.h"
#include "../../../../src/include/apps/doom.h"
#include "../../../../src/include/timer.h"
#include "../../../../src/include/thread.h"
#include "../../../../src/include/window.h"
#include "../../../../src/include/wm.h"
#include "../../../../src/include/video.h"

// Phase 2 (video) and Phase 3 (input, keyboard only -- no mouse yet).
//
// DG_ScreenBuffer is a DOOMGENERIC_RESX x DOOMGENERIC_RESY array of
// 32-bit 0x00RRGGBB pixels (see Makefile's -D flags, which pin these to
// Doom's native 320x200 -- fb_scaling works out to 1 in i_video.c's
// I_InitGraphics, the simplest case: no scaling, no x/y_offset
// letterboxing, straight 1:1 copy from doomgeneric's own internal
// buffer). This kernel's screen is 16-color planar VGA (see
// video.h/video.c), so every frame needs an RGB888 -> nearest-EGA-index
// reduction; there's no direct-color mode to fall back to.
//
// doom_get_active_window() (doom_app.c) is the only thing shared with
// that file: this file owns *how* a frame gets drawn, doom_app.c owns
// *where* (the live window, if any) and the thread that drives the
// engine forward.

// Standard VGA/EGA 16-color DAC palette -- the BIOS default that
// bootloader.asm's `int 10h, ax=0x0012` (Mode 12h) already programs, not
// something this kernel configures itself. Nearest-color match against
// this table is the only palette this port has.
static const unsigned char g_ega_rgb[16][3] = {
	{  0,   0,   0}, {  0,   0, 170}, {  0, 170,   0}, {  0, 170, 170},
	{170,   0,   0}, {170,   0, 170}, {170,  85,   0}, {170, 170, 170},
	{ 85,  85,  85}, { 85,  85, 255}, { 85, 255,  85}, { 85, 255, 255},
	{255,  85,  85}, {255,  85, 255}, {255, 255,  85}, {255, 255, 255},
};

static unsigned char nearest_ega(unsigned char r, unsigned char g, unsigned char b) {
	int best = 0;
	int best_dist = 0x7fffffff;
	for (int i = 0; i < 16; i++) {
		int dr = (int)r - g_ega_rgb[i][0];
		int dg = (int)g - g_ega_rgb[i][1];
		int db = (int)b - g_ega_rgb[i][2];
		int dist = dr * dr + dg * dg + db * db;
		if (dist < best_dist) {
			best_dist = dist;
			best = i;
		}
	}
	return (unsigned char)best;
}

void DG_Init(void) {
	// DG_ScreenBuffer is already malloc'd by doomgeneric_Create() (the
	// generic caller, see doomgeneric.c) by the time this runs -- nothing
	// else to set up on this kernel.
}

void DG_DrawFrame(void) {
	wm_window_st *win = doom_get_active_window();
	if (!win) {
		return; // window closed -- see doom_app.c's doom_on_close
	}
	if (win->state == WIN_STATE_MINIMIZED) {
		return; // nobody can see it; skip the (slow) blit
	}

	rect_st content = window_content_rect(&win->base);

	// Doom's buffer is a fixed 320x200; the window can be resized larger
	// (min DOOM_MIN_W/H, see doom.h) but this phase doesn't scale up to
	// fill it, just clips to whichever is smaller -- matches the rest of
	// this port's "get it visible and correct first" phasing.
	int w = content.w < DOOMGENERIC_RESX ? content.w : DOOMGENERIC_RESX;
	int h = content.h < DOOMGENERIC_RESY ? content.h : DOOMGENERIC_RESY;
	if (w <= 0 || h <= 0) {
		return;
	}

	for (int y = 0; y < h; y++) {
		pixel_t *row = DG_ScreenBuffer + (unsigned int)y * DOOMGENERIC_RESX;
		int py = content.y + y;
		for (int x = 0; x < w; x++) {
			pixel_t p = row[x];
			unsigned char r = (unsigned char)(p >> 16);
			unsigned char g = (unsigned char)(p >> 8);
			unsigned char b = (unsigned char)p;
			draw_pixel(content.x + x, py, nearest_ega(r, g, b));
		}
	}
}

// This kernel's PIT runs at 100Hz (see kernel.c's timer_init(100) call),
// so one tick is exactly 10ms -- both of these convert against that fixed
// rate, not a runtime-queried one (nothing in timer.h exposes the
// frequency itself).
uint32_t DG_GetTicksMs(void) {
	return timer_get_ticks() * 10u;
}

void DG_SleepMs(uint32_t ms) {
	// No timed-sleep primitive exists yet (thread.h's THREAD_SLEEPING
	// state/wake_tick field are reserved for one but scheduler_tick()
	// doesn't act on them -- see thread.c). A yield-until-tick spin is
	// simple, correct, and doesn't need to touch the scheduler for this
	// phase: thread_yield() still hands the CPU to any other READY
	// thread (the main/WM thread) while waiting.
	unsigned int target = timer_get_ticks() + (ms + 9u) / 10u;
	while (timer_get_ticks() < target) {
		thread_yield();
	}
}

int DG_GetKey(int *pressed, unsigned char *doomKey) {
	// Phase 3 (input): the actual queue/translation lives in doom_app.c,
	// right next to the on_key_down/on_key_up callbacks that fill it (see
	// doom.h's doom_dequeue_key() for the full contract).
	return doom_dequeue_key(pressed, doomKey);
}

void DG_SetWindowTitle(const char *title) {
	(void)title; // this WM's window titles are static strings, set at
	             // wm_create_window() time (see doom_app.c) -- no per-frame
	             // retitle support exists to hook this into.
}
