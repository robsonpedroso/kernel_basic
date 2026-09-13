#include "doomgeneric/doomgeneric.h"
#include "../../../../src/include/apps/doom.h"
#include "../../../../src/include/video.h"
#include "../../../../src/include/gui.h"
#include "../../../../src/include/heap.h"
#include "../../../../src/include/thread.h"
#include "../../../../src/include/wad_disk.h"
// KEY_LEFT/RIGHT/UP/DOWN/CTRL/SHIFT/ALT: this kernel's own pseudo-ascii
// codes for non-printable keys (see keyboard.h), the vocabulary
// on_key_down/on_key_up below actually receive. KEY_HOME/KEY_END collide
// by name with doomkeys.h's OWN (differently-valued) KEY_HOME/KEY_END --
// #undef them before pulling doomkeys.h in so its definitions win for the
// translation table's *target* side; Home/End aren't translated to a Doom
// key in this phase, so losing our own names here costs nothing.
#include "../../../../src/include/keyboard.h"
#undef KEY_HOME
#undef KEY_END
#include "doomgeneric/doomkeys.h"

// Phase 0's smoke-test stub proved the build path, app_st registration and
// window plumbing end-to-end. Phase 2 (video) replaces the placeholder
// paint with the real thing: doomgeneric's own DG_DrawFrame (see
// doomgeneric_rsystemos.c) now draws directly into this window's content
// rect every time the Doom engine finishes a frame, on its own kernel
// thread -- doom_on_show below only ever draws the one-time "loading"
// placeholder seen during the (multi-second) WAD/table init that happens
// before the first real frame, UNLESS the pre-flight WAD check below
// already knows there's nothing to wait for.
typedef struct {
	int wad_present; // see doom_check_wad_present()
} doom_state_t;

// The single live Doom window, or NULL -- see doom.h's declaration for the
// full lifetime/threading rationale.
static wm_window_st *g_doom_win = 0;

// The engine itself (D_DoomMain/D_DoomLoop, reached through
// doomgeneric_Create -- see doomgeneric.c) never returns under normal
// play, so it needs a dedicated kernel thread rather than blocking
// whichever thread calls doom_on_init (the main/WM thread, which the rest
// of the desktop depends on staying responsive). Started at most once:
// closing and reopening the Doom window creates a new window/app_state
// but must NOT spawn a second engine instance on top of the (still
// running, un-killable -- no thread_kill exists) first one.
static int g_doom_thread_started = 0;

// Result of doom_check_wad_present(), cached by doom_precheck_wad() (see
// below) instead of being read fresh inside doom_on_init(). Defaults to
// "not present" until the boot-time precheck actually runs.
static int g_wad_present = 0;

wm_window_st *doom_get_active_window(void) {
	return g_doom_win;
}

// Phase 3 (input). doom_on_key_down/up (below) are the producer, called
// from the "wm" thread (see kernel.c's wm_thread_main, itself fed by
// keyboard.c's IRQ handler through event.c's queue); DG_GetKey
// (doomgeneric_rsystemos.c) is the consumer, polled from the "doom"
// thread. A tiny ring buffer, same shape as every doomgeneric_*.c
// backend's own s_KeyQueue (see e.g. doomgeneric_sdl.c) -- 16 entries is
// their number too, and this port has no reason to need more headroom
// than a real desktop's event queue does. preempt_disable/enable make
// each push/pop atomic against the other thread, same pattern as
// storage_thread.c's own queue.
#define DOOM_KEYQUEUE_SIZE 16
static unsigned short g_key_queue[DOOM_KEYQUEUE_SIZE];
static unsigned int g_key_queue_write = 0;
static unsigned int g_key_queue_read = 0;

static void doom_enqueue_key(int pressed, unsigned char doom_key) {
	preempt_disable();
	g_key_queue[g_key_queue_write] = (unsigned short)((pressed ? 0x100 : 0) | doom_key);
	g_key_queue_write = (g_key_queue_write + 1) % DOOM_KEYQUEUE_SIZE;
	// No overflow guard: a full queue silently overwrites its oldest
	// unread entry (write catches up to read) rather than blocking or
	// growing -- matches the reference backends this was modeled on, and
	// 16 slots is generous next to how fast a human can press keys versus
	// how often the Doom thread's own loop calls DG_GetKey.
	preempt_enable();
}

int doom_dequeue_key(int *pressed, unsigned char *doomKey) {
	preempt_disable();
	if (g_key_queue_read == g_key_queue_write) {
		preempt_enable();
		return 0;
	}
	unsigned short entry = g_key_queue[g_key_queue_read];
	g_key_queue_read = (g_key_queue_read + 1) % DOOM_KEYQUEUE_SIZE;
	preempt_enable();

	*pressed = (entry & 0x100) ? 1 : 0;
	*doomKey = (unsigned char)(entry & 0xFF);
	return 1;
}

// Mirrors every doomgeneric_*.c backend's own convertToDoomKey (see e.g.
// doomgeneric_sdl.c) against this kernel's own key vocabulary instead of
// SDL's: arrows/Ctrl/Space/Shift/Alt map to Doom's named action keys
// (fire/use/run/strafe-modifier), Enter/Escape pass through as themselves
// (already the same ASCII values doomkeys.h expects), and everything else
// low-ASCII passes through unchanged after lower-casing -- this kernel's
// own keyboard.c already shift-uppercases letters (see its
// keyboard_ascii_shift table), which Doom doesn't want: KEY_RSHIFT above
// is what tells it Shift is held, a letter key's *case* isn't meaningful
// to it. F-keys/Home/End/PageUp/Down aren't wired to a scancode->ascii
// mapping in this kernel's keyboard.c at all yet, so they're simply never
// seen here -- not a Doom-specific gap.
static unsigned char translate_to_doom_key(int ascii) {
	if (ascii >= 'A' && ascii <= 'Z') {
		ascii += 'a' - 'A';
	}
	switch (ascii) {
		case KEY_LEFT:  return KEY_LEFTARROW;
		case KEY_RIGHT: return KEY_RIGHTARROW;
		case KEY_UP:    return KEY_UPARROW;
		case KEY_DOWN:  return KEY_DOWNARROW;
		case KEY_CTRL:  return KEY_FIRE;
		case KEY_SHIFT: return KEY_RSHIFT;
		case KEY_ALT:   return KEY_LALT;
		case ' ':       return KEY_USE;
		case '=': case '+': return KEY_EQUALS;
		case '-':       return KEY_MINUS;
		default:
			// Doom's own KEY_ENTER (13) and KEY_ESCAPE (27) are already
			// these exact ASCII values -- see doomkeys.h -- so '\n'/27 fall
			// through here correctly with no explicit case needed.
			if (ascii >= 0 && ascii < 128) {
				return (unsigned char)ascii;
			}
			return 0; // unmapped (e.g. KEY_DELETE) -- DG_GetKey's caller
			          // already drops events whose translated key is 0.
	}
}

static void doom_on_key_down(wm_window_st *win, void *state, int ascii, int mods) {
	(void)win;
	(void)state;
	(void)mods;
	doom_enqueue_key(1, translate_to_doom_key(ascii));
}

static void doom_on_key_up(wm_window_st *win, void *state, int ascii, int mods) {
	(void)win;
	(void)state;
	(void)mods;
	doom_enqueue_key(0, translate_to_doom_key(ascii));
}

// Without a real apps/games/src/doom/wads/doom1.wad present at build time
// (see Makefile), the on-disk WAD region (wad_disk.h's WAD_LBA) is just
// whatever the image was zero-filled with -- doomgeneric_Create() still
// runs, gets all the way into D_DoomMain, and only THEN discovers this
// deep inside w_wad.c's W_AddFile (the on-disk header doesn't start with
// "IWAD"/"PWAD"), at which point it calls I_Error() and halts (see
// doom_libc_shim.c's exit()) -- silently as far as this window is
// concerned, since nothing there ever talks back to doom_app.c. The net
// effect used to be exactly what it looks like from the desktop: stuck on
// "carregando..." forever, with the actual reason visible only on the
// serial log nobody's watching.
//
// Checking the region's first 4 bytes here, before ever spawning the
// engine thread, turns that into an immediate, on-screen, correct
// diagnosis instead -- and skips paying for the (multi-second) engine
// bring-up just to watch it fail at the very end regardless.
static int doom_check_wad_present(void) {
	unsigned char sig[4];
	if (wad_read_range(0, sig, sizeof(sig)) != 0) {
		return 0;
	}
	// Same check as doomgeneric/w_wad.c's W_AddFile: identification must
	// be "IWAD" or "PWAD".
	if (sig[1] != 'W' || sig[2] != 'A' || sig[3] != 'D') {
		return 0;
	}
	return sig[0] == 'I' || sig[0] == 'P';
}

// Must run before any window can exist (called from kernel_init(), see
// kernel.c), NOT lazily from doom_on_init(). wad_read_range() ->
// storage_read_sectors() blocks the calling thread on the storage worker's
// queue (storage_thread.c's submit(), via thread_block()) -- and
// thread_block()'s trailing sti (thread.c) fires unconditionally on
// resume, regardless of any outer preempt_disable() nesting. wm.c's
// wm_create_window() wraps the entire app->on_init(w) call in
// preempt_disable()/preempt_enable() specifically to make it atomic w.r.t.
// the scheduler (see that function's own comment) -- doing this blocking
// read from inside doom_on_init defeated that: the storage round-trip's
// early sti re-enabled interrupts mid-on_init, reopening the exact
// preemption race that fix closed, just via a different path than before.
// This caused a real, reproduced freeze (window creation losing the "wm"
// thread from the scheduler) as soon as a real doom1.wad made this check
// actually block on a real disk read instead of failing instantly.
// Precomputing it here, before "wm" thread even starts dispatching clicks,
// means doom_on_init() never blocks on anything.
void doom_precheck_wad(void) {
	g_wad_present = doom_check_wad_present();
}

static void doom_thread_entry(void *arg) {
	(void)arg;
	doomgeneric_Create(0, 0);
	// Only reached if D_DoomMain ever returns instead of looping forever
	// or halting via exit() (see doom_libc_shim.c) -- nothing sensible
	// left to do on this thread at that point.
	thread_exit();
}

// 256KiB: bumped from an initial 64KiB guess after a real crash looked
// exactly stack-shaped (Terminal faulted on a later, unrelated kmalloc
// after Doom had actually run a frame) -- classic Doom's renderer (BSP
// traversal, visplane/sprite sorting) recurses and uses stack-local
// buffers, and this port has no stack-overflow guard page to turn a
// too-small guess into a clean failure instead of silent heap corruption
// of whatever kmalloc'd block happens to follow this one. Still a guess
// with headroom, not a measurement -- if a crash after running Doom ever
// recurs, get the panic's eip (see isr.c) and check whether it's inside
// this stack's kmalloc'd range before assuming it needs to grow further.
#define DOOM_THREAD_STACK_SIZE (256u * 1024u)

static void *doom_on_init(wm_window_st *win) {
	g_doom_win = win;

	doom_state_t *state = (doom_state_t *)kmalloc(sizeof(doom_state_t));
	state->wad_present = g_wad_present; // see doom_precheck_wad()

	// Only spawn the (un-killable, see g_doom_thread_started's own comment)
	// engine thread if there's an actual chance of it doing something --
	// no point paying for D_DoomMain's bring-up just to watch it discover
	// the same thing doom_check_wad_present() already knows for free.
	if (state->wad_present && !g_doom_thread_started) {
		g_doom_thread_started = 1;
		thread_create("doom", doom_thread_entry, 0, DOOM_THREAD_STACK_SIZE);
	}
	return state;
}

static void doom_on_show(wm_window_st *win, void *state_ptr, rect_st content) {
	(void)win;
	doom_state_t *state = (doom_state_t *)state_ptr;

	fill_rect(content.x, content.y, content.w, content.h, GUI_COLOR_TITLE);
	if (!state->wad_present) {
		// Permanent -- there is no runtime way to supply a WAD (it's baked
		// into the disk image at build time only, see Makefile), so this
		// never gets overdrawn by DG_DrawFrame the way the loading message
		// below does.
		draw_text(content.x + 8, content.y + 8,  "DOOM: doom1.wad nao encontrado", GUI_COLOR_TITLE_TEXT);
		draw_text(content.x + 8, content.y + 20, "Coloque em apps/games/src/doom/wads/",  GUI_COLOR_TITLE_TEXT);
		draw_text(content.x + 8, content.y + 32, "e rode make distclean && make all.",     GUI_COLOR_TITLE_TEXT);
		return;
	}

	// One-time placeholder: DG_DrawFrame (doomgeneric_rsystemos.c) takes
	// over this same content rect, from the Doom thread, as soon as the
	// engine produces its first real frame. Until then (WAD load, table
	// init -- can take a few seconds) this is what's on screen.
	draw_text(content.x + 8, content.y + 8, "DOOM (carregando...)", GUI_COLOR_TITLE_TEXT);
}

static void doom_on_close(wm_window_st *win, void *state) {
	(void)win;
	// g_doom_win must go stale before wm_close_window()'s kfree(win) runs
	// (this IS that call, one frame up the stack) -- DG_DrawFrame checks
	// it first thing and skips the frame instead of touching freed
	// memory. The Doom thread itself keeps running in the background
	// (see g_doom_thread_started's comment): it just has nowhere left to
	// draw.
	g_doom_win = 0;
	kfree(state);
}

const app_st doom_app = {
	.name = "Doom",
	.on_init = doom_on_init,
	.on_show = doom_on_show,
	.on_key_down = doom_on_key_down,
	.on_key_up = doom_on_key_up,
	.on_mouse_down = 0,
	.on_mouse_up = 0,
	.on_mouse_move = 0,
	.on_tick = 0,
	.on_close = doom_on_close,
};
