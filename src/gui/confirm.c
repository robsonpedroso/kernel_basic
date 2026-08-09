#include "../include/confirm.h"
#include "../include/video.h"
#include "../include/gui.h"
#include "../include/widget.h"

#define CONFIRM_BOX_H  48
#define CONFIRM_BTN_W  60
#define CONFIRM_BTN_H  20

void confirm_open(confirm_st *c, const char *message, int pending_action) {
	int i = 0;
	while (message[i] && i < CONFIRM_MSG_MAX - 1) {
		c->message[i] = message[i];
		i++;
	}
	c->message[i] = 0;
	c->pending_action = pending_action;
	c->active = 1;
}

void confirm_close(confirm_st *c) {
	c->active = 0;
}

static rect_st confirm_box_rect(rect_st content) {
	rect_st r = { content.x, content.y + content.h - CONFIRM_BOX_H, content.w, CONFIRM_BOX_H };
	return r;
}

static rect_st confirm_yes_rect(rect_st content) {
	rect_st box = confirm_box_rect(content);
	rect_st r = { box.x + box.w / 2 - CONFIRM_BTN_W - 8, box.y + box.h - CONFIRM_BTN_H - 4, CONFIRM_BTN_W, CONFIRM_BTN_H };
	return r;
}

static rect_st confirm_no_rect(rect_st content) {
	rect_st box = confirm_box_rect(content);
	rect_st r = { box.x + box.w / 2 + 8, box.y + box.h - CONFIRM_BTN_H - 4, CONFIRM_BTN_W, CONFIRM_BTN_H };
	return r;
}

void confirm_draw(confirm_st *c, rect_st content) {
	if (!c->active) {
		return;
	}
	rect_st box = confirm_box_rect(content);
	fill_rect(box.x, box.y, box.w, box.h, GUI_COLOR_BG);
	draw_bevel(box.x, box.y, box.w, box.h, 0);

	draw_text(box.x + 4, box.y + 4, c->message, GUI_COLOR_BLACK);

	widget_st yes = { confirm_yes_rect(content), "Sim", 0, 0 };
	widget_st no  = { confirm_no_rect(content), "Nao", 0, 0 };
	widget_button_draw(&yes);
	widget_button_draw(&no);
}

int confirm_mouse_down(confirm_st *c, rect_st content, int lx, int ly) {
	if (!c->active) {
		return 0;
	}
	rect_st yes = confirm_yes_rect(content);
	rect_st no = confirm_no_rect(content);
	if (rect_contains_point(&yes, lx, ly)) {
		confirm_close(c);
		return 1;
	}
	if (rect_contains_point(&no, lx, ly)) {
		confirm_close(c);
		return -1;
	}
	return 0;
}

int confirm_key_down(confirm_st *c, int ascii) {
	if (!c->active) {
		return 0;
	}
	if (ascii == '\n') {
		confirm_close(c);
		return 1;
	}
	if (ascii == 27) {
		confirm_close(c);
		return -1;
	}
	return 0;
}
