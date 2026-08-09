#include "../../include/apps/text_editor.h"
#include "../../include/wm.h"
#include "../../include/window.h"
#include "../../include/editbuf.h"
#include "../../include/lineedit.h"
#include "../../include/textbox.h"
#include "../../include/widget.h"
#include "../../include/video.h"
#include "../../include/gui.h"
#include "../../include/heap.h"
#include "../../include/fs.h"
#include "../../include/timer.h"
#include "../../include/keyboard.h"
#include "../../include/event.h"

#define TE_COLS 53
#define TE_ROWS 20
#define TE_PAD 4
#define TE_TOOLBAR_H 28
#define TE_BLINK_TICKS 50 // ~500ms @ 100Hz, same as terminal.c

typedef struct {
	editbuf_st  eb;
	textbox_st  tb;         // render target only, sized once in on_init
	lineedit_st prompt;
	int  file_id;           // -1 = untitled
	int  save_parent;       // folder to create the file in if untitled
	rect_st save_btn;       // content-relative, refreshed every on_show
	char status[40];
	int  blink_on;
	unsigned int last_blink_tick;
} text_editor_state_t;

static int g_pending_file_id = -1;
static int g_pending_parent = FS_ROOT;

void text_editor_set_target(int file_id, int parent) {
	g_pending_file_id = file_id;
	g_pending_parent = parent;
}

// string.c's strcpy never writes the terminator -- write our own that does.
static void te_status_copy(text_editor_state_t *s, const char *msg) {
	int i = 0;
	while (msg[i] && i < (int)sizeof(s->status) - 1) {
		s->status[i] = msg[i];
		i++;
	}
	s->status[i] = 0;
}

static void te_save(text_editor_state_t *s) {
	if (s->file_id >= 0) {
		if (fs_write_file(s->file_id, s->eb.data, s->eb.len) == 0) {
			s->eb.dirty = 0;
			te_status_copy(s, "salvo");
		} else {
			te_status_copy(s, "erro ao salvar (sem espaco?)");
		}
		return;
	}
	lineedit_open(&s->prompt, "Salvar como:");
}

static void te_save_as(text_editor_state_t *s, const char *name) {
	int id = fs_create_file(s->save_parent, name);
	if (id < 0) {
		te_status_copy(s, "nome invalido ou repetido");
		return;
	}
	s->file_id = id;
	te_save(s);
}

static void *text_editor_on_init(wm_window_st *win) {
	(void)win;
	text_editor_state_t *s = (text_editor_state_t *)kmalloc(sizeof(text_editor_state_t));
	editbuf_init(&s->eb, EDITBUF_CAPACITY);
	textbox_init(&s->tb, TE_COLS, TE_ROWS);
	lineedit_close(&s->prompt);
	s->blink_on = 1;
	s->last_blink_tick = 0;

	s->file_id = g_pending_file_id;
	s->save_parent = g_pending_parent;
	g_pending_file_id = -1;
	g_pending_parent = FS_ROOT;

	if (s->file_id >= 0) {
		int n = fs_read_file(s->file_id, s->eb.data, s->eb.cap);
		s->eb.len = (n > 0) ? n : 0;
		s->eb.cursor = 0;
		s->eb.goal_col = -1;
		s->eb.top_line = 0;
		s->eb.dirty = 0;
		const fs_dirent_t *e = fs_get(s->file_id);
		te_status_copy(s, e ? e->name : "");
	} else {
		te_status_copy(s, "(sem titulo)");
	}
	return s;
}

static void text_editor_on_show(wm_window_st *win, void *state, rect_st content) {
	(void)win;
	text_editor_state_t *s = (text_editor_state_t *)state;
	fill_rect(content.x, content.y, content.w, content.h, GUI_COLOR_BG);

	s->save_btn.x = 4;
	s->save_btn.y = 4;
	s->save_btn.w = 72;
	s->save_btn.h = 20;

	widget_st btn;
	btn.rect.x = content.x + s->save_btn.x;
	btn.rect.y = content.y + s->save_btn.y;
	btn.rect.w = s->save_btn.w;
	btn.rect.h = s->save_btn.h;
	btn.label = "Salvar";
	btn.selected = 0;
	btn.pressed = 0;
	widget_button_draw(&btn);

	draw_text(content.x + s->save_btn.x + s->save_btn.w + 8,
	          content.y + s->save_btn.y + (s->save_btn.h - FONT_H) / 2,
	          s->status, GUI_COLOR_BLACK);

	rect_st edit_area = { content.x, content.y + TE_TOOLBAR_H, content.w, content.h - TE_TOOLBAR_H };
	fill_rect(edit_area.x, edit_area.y, edit_area.w, edit_area.h, GUI_COLOR_BLACK);
	editbuf_render(&s->eb, &s->tb);
	textbox_draw(&s->tb, edit_area.x + TE_PAD, edit_area.y + TE_PAD, s->blink_on);

	if (s->prompt.active) {
		lineedit_draw(&s->prompt, content);
	}
}

static void text_editor_on_key_down(wm_window_st *win, void *state, int ascii, int mods) {
	(void)win;
	text_editor_state_t *s = (text_editor_state_t *)state;

	if (s->prompt.active) {
		int r = lineedit_key(&s->prompt, ascii);
		if (r == 1) {
			te_save_as(s, s->prompt.text);
			lineedit_close(&s->prompt);
		}
		return;
	}

	if ((mods & KEY_MOD_CTRL) && (ascii == 's' || ascii == 'S')) {
		te_save(s);
		return;
	}

	editbuf_key(&s->eb, ascii);
}

static void text_editor_on_mouse_down(wm_window_st *win, void *state, int lx, int ly, int buttons) {
	(void)win;
	(void)buttons;
	text_editor_state_t *s = (text_editor_state_t *)state;
	if (s->prompt.active) {
		return; // ignore clicks while naming
	}
	if (rect_contains_point(&s->save_btn, lx, ly)) {
		te_save(s);
	}
}

static int text_editor_on_tick(wm_window_st *win, void *state) {
	(void)win;
	text_editor_state_t *s = (text_editor_state_t *)state;
	unsigned int now = timer_get_ticks();
	if (now - s->last_blink_tick >= TE_BLINK_TICKS) {
		s->blink_on = !s->blink_on;
		s->last_blink_tick = now;
		return 1;
	}
	return 0;
}

static void text_editor_on_close(wm_window_st *win, void *state) {
	(void)win;
	text_editor_state_t *s = (text_editor_state_t *)state;
	editbuf_free(&s->eb);
	textbox_free(&s->tb);
	kfree(s);
}

const app_st text_editor_app = {
	.name = "Text Editor",
	.icon = ICON_TEXT_EDITOR,
	.on_init = text_editor_on_init,
	.on_show = text_editor_on_show,
	.on_key_down = text_editor_on_key_down,
	.on_mouse_down = text_editor_on_mouse_down,
	.on_mouse_up = 0,
	.on_mouse_move = 0,
	.on_tick = text_editor_on_tick,
	.on_close = text_editor_on_close,
};
