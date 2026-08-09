#include "../../include/apps/calculator.h"
#include "../../include/wm.h"
#include "../../include/widget.h"
#include "../../include/rect.h"
#include "../../include/video.h"
#include "../../include/gui.h"
#include "../../include/heap.h"
#include "../../include/string.h"
#include "../../include/stdlib.h"

#define CALC_ROWS 4
#define CALC_COLS 4
#define CALC_DISPLAY_H 32

static const char * const g_calc_labels[CALC_ROWS][CALC_COLS] = {
	{ "7", "8", "9", "/" },
	{ "4", "5", "6", "*" },
	{ "1", "2", "3", "-" },
	{ "0", "C", "=", "+" },
};

typedef struct {
	rect_st btn_rects[CALC_ROWS][CALC_COLS];   // content-relative, refreshed every on_show
	char display[16];
	int display_len;
	long acc;
	char pending_op;   // 0 = none
	int entry_active;  // 1 while typing a fresh operand
} calculator_state_t;

static void *calculator_on_init(wm_window_st *win) {
	(void)win;
	calculator_state_t *s = (calculator_state_t *)kmalloc(sizeof(calculator_state_t));
	s->display[0] = '0';
	s->display[1] = 0;
	s->display_len = 1;
	s->acc = 0;
	s->pending_op = 0;
	s->entry_active = 1;
	return s;
}

static void calc_append_digit(calculator_state_t *s, char d) {
	if (!s->entry_active) {
		s->display_len = 0;
		s->entry_active = 1;
	}
	if (s->display_len == 1 && s->display[0] == '0') {
		s->display_len = 0;
	}
	if (s->display_len < (int)sizeof(s->display) - 1) {
		s->display[s->display_len++] = d;
		s->display[s->display_len] = 0;
	}
}

// Returns 1 on divide-by-zero (display already set to "Err" -- caller must
// not overwrite it with itoa(acc) afterward), 0 otherwise.
static int calc_apply_pending(calculator_state_t *s) {
	long rhs = atoi(s->display);
	if (s->pending_op == 0) {
		s->acc = rhs;
		return 0;
	}
	switch (s->pending_op) {
		case '+':
			s->acc += rhs;
			break;
		case '-':
			s->acc -= rhs;
			break;
		case '*':
			s->acc *= rhs;
			break;
		case '/':
			if (rhs == 0) {
				strcpy(s->display, "Err");
				s->display_len = 3;
				s->acc = 0;
				s->pending_op = 0;
				s->entry_active = 0;
				return 1;
			}
			s->acc /= rhs;
			break;
	}
	return 0;
}

static void calc_press(calculator_state_t *s, const char *label) {
	if (label[1] == 0 && label[0] >= '0' && label[0] <= '9') {
		calc_append_digit(s, label[0]);
		return;
	}
	if (strcmp((char *)label, "C") == 0) {
		s->acc = 0;
		s->pending_op = 0;
		s->display[0] = '0';
		s->display[1] = 0;
		s->display_len = 1;
		s->entry_active = 1;
		return;
	}
	if (strcmp((char *)label, "=") == 0) {
		if (calc_apply_pending(s)) return;
		s->pending_op = 0;
		itoa(s->display, (int)s->acc);
		s->display_len = strlen(s->display);
		s->entry_active = 0;
		return;
	}
	// operator
	if (calc_apply_pending(s)) return;
	itoa(s->display, (int)s->acc);
	s->display_len = strlen(s->display);
	s->pending_op = label[0];
	s->entry_active = 0;
}

static void calculator_on_show(wm_window_st *win, void *state, rect_st content) {
	(void)win;
	calculator_state_t *s = (calculator_state_t *)state;
	fill_rect(content.x, content.y, content.w, content.h, GUI_COLOR_BG);

	rect_st disp = { content.x + 4, content.y + 4, content.w - 8, CALC_DISPLAY_H };
	fill_rect(disp.x, disp.y, disp.w, disp.h, GUI_COLOR_LIGHT);
	draw_bevel(disp.x, disp.y, disp.w, disp.h, 1);
	int tw = s->display_len * FONT_W;
	draw_text(disp.x + disp.w - 8 - tw, disp.y + (disp.h - FONT_H) / 2, s->display, GUI_COLOR_BLACK);

	// Button grid, laid out fresh from the *current* content size every
	// call -- there is no on_resize hook, so this is what makes live
	// window resize "just work".
	int grid_y = content.y + 4 + CALC_DISPLAY_H + 4;
	int grid_w = content.w - 8;
	int grid_h = content.h - (grid_y - content.y) - 4;
	int cell_w = grid_w / CALC_COLS;
	int cell_h = grid_h / CALC_ROWS;

	for (int r = 0; r < CALC_ROWS; r++) {
		for (int c = 0; c < CALC_COLS; c++) {
			rect_st cell;
			cell.x = 4 + c * cell_w;               // content-relative
			cell.y = (grid_y - content.y) + r * cell_h;
			cell.w = cell_w - 2;
			cell.h = cell_h - 2;
			s->btn_rects[r][c] = cell;

			widget_st btn;
			btn.rect.x = content.x + cell.x;
			btn.rect.y = content.y + cell.y;
			btn.rect.w = cell.w;
			btn.rect.h = cell.h;
			btn.label = g_calc_labels[r][c];
			btn.selected = 0;
			btn.pressed = 0;
			widget_button_draw(&btn);
		}
	}
}

static void calculator_on_mouse_down(wm_window_st *win, void *state, int lx, int ly, int buttons) {
	(void)win;
	(void)buttons;
	calculator_state_t *s = (calculator_state_t *)state;
	for (int r = 0; r < CALC_ROWS; r++) {
		for (int c = 0; c < CALC_COLS; c++) {
			if (rect_contains_point(&s->btn_rects[r][c], lx, ly)) {
				calc_press(s, g_calc_labels[r][c]);
				return;
			}
		}
	}
}

static void calculator_on_close(wm_window_st *win, void *state) {
	(void)win;
	kfree(state);
}

const app_st calculator_app = {
	.name = "Calculator",
	.icon = ICON_CALCULATOR,
	.on_init = calculator_on_init,
	.on_show = calculator_on_show,
	.on_key_down = 0,
	.on_mouse_down = calculator_on_mouse_down,
	.on_mouse_up = 0,
	.on_mouse_move = 0,
	.on_tick = 0,
	.on_close = calculator_on_close,
};
