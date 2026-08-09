#include "../include/mouse.h"
#include "../include/isr.h"
#include "../include/event.h"
#include "../include/io.h"
#include "../include/video.h"

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_COMMAND 0x64

#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL  0x02

static int mouse_x = SCREEN_W / 2;
static int mouse_y = SCREEN_H / 2;
static unsigned char packet[3];
static int packet_index = 0;
static unsigned char last_buttons = 0;

static void wait_input_clear(void) {
	int timeout = 100000;
	while (timeout-- > 0 && (inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL)) { }
}

static void wait_output_full(void) {
	int timeout = 100000;
	while (timeout-- > 0 && !(inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)) { }
}

static void mouse_write(unsigned char value) {
	wait_input_clear();
	outb(PS2_COMMAND, 0xD4); // "next data byte is for the second PS/2 port"
	wait_input_clear();
	outb(PS2_DATA, value);
}

static unsigned char mouse_read(void) {
	wait_output_full();
	return inb(PS2_DATA);
}

static void clamp_cursor(void) {
	if (mouse_x < 0) mouse_x = 0;
	if (mouse_y < 0) mouse_y = 0;
	if (mouse_x >= SCREEN_W) mouse_x = SCREEN_W - 1;
	if (mouse_y >= SCREEN_H) mouse_y = SCREEN_H - 1;
}

static void mouse_irq_handler(registers_t *regs) {
	(void)regs;

	unsigned char data = inb(PS2_DATA);

	if (packet_index == 0 && !(data & 0x08)) {
		return; // alignment bit not set: we're out of sync, drop until byte 0
	}

	packet[packet_index++] = data;
	if (packet_index < 3) {
		return;
	}
	packet_index = 0;

	unsigned char flags = packet[0];

	if ((flags & 0x40) || (flags & 0x80)) {
		return; // X or Y overflow: packet is unreliable, discard it
	}

	int dx = packet[1];
	int dy = packet[2];
	if (flags & 0x10) dx -= 256; // sign-extend 9-bit two's complement value
	if (flags & 0x20) dy -= 256;
	dy = -dy; // PS/2 reports +Y as "up"; our screen Y grows downward

	mouse_x += dx;
	mouse_y += dy;
	clamp_cursor();

	if (dx != 0 || dy != 0) {
		event_push(EVENT_MOUSE_MOVE, dx, dy);
	}

	unsigned char buttons = flags & (MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT);
	unsigned char pressed = buttons & (unsigned char)~last_buttons;
	unsigned char released = last_buttons & (unsigned char)~buttons;
	if (pressed) {
		event_push(EVENT_MOUSE_DOWN, pressed, 0);
	}
	if (released) {
		event_push(EVENT_MOUSE_UP, released, 0);
	}
	last_buttons = buttons;
}

void mouse_init(void) {
	wait_input_clear();
	outb(PS2_COMMAND, 0xA8); // enable the auxiliary (mouse) device

	wait_input_clear();
	outb(PS2_COMMAND, 0x20); // read controller configuration byte
	wait_output_full();
	unsigned char config = inb(PS2_DATA);

	config |= 0x02;  // enable IRQ12 (second PS/2 port interrupt)
	config &= ~0x20; // ensure the second PS/2 port clock is enabled

	wait_input_clear();
	outb(PS2_COMMAND, 0x60); // write controller configuration byte
	wait_input_clear();
	outb(PS2_DATA, config);

	mouse_write(0xF6); // set defaults
	mouse_read();       // ack (0xFA)

	mouse_write(0xF4); // enable data reporting
	mouse_read();       // ack (0xFA)

	packet_index = 0;
	last_buttons = 0;
	mouse_x = SCREEN_W / 2;
	mouse_y = SCREEN_H / 2;

	irq_install_handler(12, mouse_irq_handler);
}

int mouse_get_x(void) { return mouse_x; }
int mouse_get_y(void) { return mouse_y; }
