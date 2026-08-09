#ifndef _EVENT_H
#define _EVENT_H

typedef enum {
	EVENT_NONE = 0,
	EVENT_KEY_DOWN,
	EVENT_KEY_UP,
	EVENT_MOUSE_MOVE,
	EVENT_MOUSE_DOWN,
	EVENT_MOUSE_UP,
	EVENT_TIMER_TICK,
} event_type_t;

// Modifier bits, used in key_st.b (EVENT_KEY_DOWN / EVENT_KEY_UP).
#define KEY_MOD_SHIFT 0x01
#define KEY_MOD_CTRL  0x02
#define KEY_MOD_ALT   0x04

// Button bits, used in mouse_st.a (EVENT_MOUSE_DOWN / EVENT_MOUSE_UP).
#define MOUSE_BUTTON_LEFT  0x01
#define MOUSE_BUTTON_RIGHT 0x02

typedef struct event {
	event_type_t type;
	int a; // KEY_*: ascii char.      MOUSE_MOVE: dx.  MOUSE_DOWN/UP: button mask.
	int b; // KEY_*: modifier mask.   MOUSE_MOVE: dy.  (unused for MOUSE_DOWN/UP)
} event_st;

void event_init(void);

// Pushes an event. Only safe to call with interrupts disabled (ISR context
// or with an explicit cli), never blocks/allocates.
void event_push(event_type_t type, int a, int b);

// Blocks (via sti+hlt) until an event is available, then pops it into *out.
void event_wait(event_st *out);

#endif
