#ifndef _KEYBOARD_H
#define _KEYBOARD_H

// Installs the IRQ1 handler (call after isr_install()). Key presses/releases
// are delivered through the event queue (event.h) as EVENT_KEY_DOWN /
// EVENT_KEY_UP {a=ascii, b=modifier mask} — there is no blocking read
// function anymore, callers use event_wait().
void keyboard_init(void);

// Live KEY_MOD_SHIFT/CTRL/ALT mask (event.h), for callbacks that need to
// know modifier state at a moment that isn't a key event -- e.g.
// on_mouse_down deciding single-select vs. Ctrl-toggle vs. Shift-range in
// a list box. app_st's mouse callbacks only carry the button mask, and
// changing their signature would touch every existing app, so this is a
// small getter instead.
unsigned int keyboard_get_mods(void);

// Non-ASCII keys, delivered through the same EVENT_KEY_DOWN {a=...} path as
// everything else. 0x80..0x86 is safe: the scancode->ASCII tables in
// keyboard.c only ever produce values 0..127 (standard ASCII), so these
// never collide with a real character.
#define KEY_LEFT   0x80
#define KEY_RIGHT  0x81
#define KEY_UP     0x82
#define KEY_DOWN   0x83
#define KEY_DELETE 0x84
#define KEY_HOME   0x85
#define KEY_END    0x86

// Ctrl/Shift/Alt as keys in their own right, not just the KEY_MOD_* bits in
// event_st.b -- added for the Doom port (apps/games/src/doom/doom_app.c),
// which needs press/release of these three specifically (fire/run/nothing
// in classic Doom's default control scheme) the same way it needs any
// other key's up/down edge. Previously these scancodes only updated
// keyboard.c's internal `mods` and returned without ever reaching
// event_push -- no existing app noticed, since none asked for them.
#define KEY_CTRL  0x87
#define KEY_SHIFT 0x88
#define KEY_ALT   0x89

#endif
