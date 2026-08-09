#ifndef _MOUSE_H
#define _MOUSE_H

// Initializes the 8042 auxiliary (PS/2 mouse) port and installs the IRQ12
// handler (call after isr_install()). Movement/buttons are delivered
// through the event queue as EVENT_MOUSE_MOVE {a=dx, b=dy} and
// EVENT_MOUSE_DOWN / EVENT_MOUSE_UP {a=button mask}.
void mouse_init(void);

// Absolute cursor position, clamped to [0, SCREEN_W-1] x [0, SCREEN_H-1].
int mouse_get_x(void);
int mouse_get_y(void);

#endif
