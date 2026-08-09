#include "../../include/apps/file_manager.h"
#include "../../include/apps/text_editor.h"
#include "../../include/wm.h"
#include "../../include/widget.h"
#include "../../include/menubar.h"
#include "../../include/treeview.h"
#include "../../include/listbox.h"
#include "../../include/scrollbar.h"
#include "../../include/splitter.h"
#include "../../include/confirm.h"
#include "../../include/lineedit.h"
#include "../../include/rect.h"
#include "../../include/video.h"
#include "../../include/gui.h"
#include "../../include/heap.h"
#include "../../include/fs.h"
#include "../../include/timer.h"
#include "../../include/keyboard.h"
#include "../../include/event.h"
#include "../../include/stdlib.h"
#include "../../include/string.h"

#define FM_DRIVEBAR_H 24
#define FM_STATUS_H   16
#define FM_PAD        4
#define FM_SCROLLBAR_W 10
#define FM_LABEL_MAX  128
#define FM_STATUS_MAX 96
// Own double-click threshold: wm.c's DOUBLE_CLICK_TICKS is static and
// unreachable, same rationale program_manager.c already documents.
#define FM_DOUBLE_CLICK_TICKS 30

#define FM_PROMPT_NONE   0
#define FM_PROMPT_FOLDER 1
#define FM_PROMPT_FILE   2
#define FM_PROMPT_RENAME 3

#define FM_ACTION_NONE    0
#define FM_ACTION_DELETE  1
#define FM_ACTION_MOVE_TO 2
#define FM_ACTION_COPY_TO 3

#define FM_SORT_NAME 0
#define FM_SORT_TYPE 1
#define FM_SORT_SIZE 2
#define FM_SORT_DATE 3

#define FM_CMD_NEW_FOLDER      1
#define FM_CMD_NEW_FILE        2
#define FM_CMD_RENAME          3
#define FM_CMD_DELETE          4
#define FM_CMD_MOVE_TO         5
#define FM_CMD_COPY_TO         6
#define FM_CMD_EXIT            7
#define FM_CMD_TREE_TOGGLE     8
#define FM_CMD_SORT_NAME       9
#define FM_CMD_SORT_TYPE       10
#define FM_CMD_SORT_SIZE       11
#define FM_CMD_SORT_DATE       12
#define FM_CMD_CONFIRM_TOGGLE  13
#define FM_CMD_CASCADE         14
#define FM_CMD_TILE            15
#define FM_CMD_ABOUT           16

// Mutable label for the Options menu's single toggle item -- menu_item_t
// tables are static const, so the "current state" text lives in a
// separate buffer the item's label pointer aims at. Safe as a lone global:
// File Manager is launched as a singleton (see program_manager.c's
// launcher table), so only one instance can ever exist at a time.
static char g_fm_confirm_label[40];

typedef struct {
	int cwd; // FS_ROOT or a folder id -- also the tree's highlighted row

	menubar_st   menubar;
	treeview_st  tree;
	scrollbar_st tree_sb;
	listbox_st   list;
	scrollbar_st list_sb;
	rect_st splitter_rect; // cached each on_show; splitter.c itself is stateless
	int split_x;            // pane_area-relative x of the splitter bar
	int dragging_splitter;

	int  ids[LISTBOX_MAX_ROWS];              // current directory listing, fs ids
	char labels_buf[LISTBOX_MAX_ROWS][FM_LABEL_MAX];
	int  count;
	int  sort_mode;

	lineedit_st prompt;
	int prompt_kind;
	int rename_target_id;

	confirm_st confirm;
	int pending_action; // FM_ACTION_MOVE_TO/COPY_TO while picking a tree destination

	int dragging; // tentative list->tree drag-and-drop, committed at mouse-up

	int confirmations_enabled;

	int last_click_row;
	unsigned int last_click_tick;

	char status[FM_STATUS_MAX];
} file_manager_state_t;

static const menu_item_t g_fm_file_items[] = {
	{ "Nova pasta",     FM_CMD_NEW_FOLDER, 1 },
	{ "Novo arquivo",   FM_CMD_NEW_FILE,   1 },
	{ "Renomear",       FM_CMD_RENAME,     1 },
	{ "Excluir",        FM_CMD_DELETE,     1 },
	{ "Mover para...",  FM_CMD_MOVE_TO,    1 },
	{ "Copiar para...", FM_CMD_COPY_TO,    1 },
	{ "Sair",           FM_CMD_EXIT,       1 },
};

static const menu_item_t g_fm_tree_items[] = {
	{ "Expandir/Recolher", FM_CMD_TREE_TOGGLE, 1 },
};

static const menu_item_t g_fm_view_items[] = {
	{ "Ordenar por nome",    FM_CMD_SORT_NAME, 1 },
	{ "Ordenar por tipo",    FM_CMD_SORT_TYPE, 1 },
	{ "Ordenar por tamanho", FM_CMD_SORT_SIZE, 1 },
	{ "Ordenar por data",    FM_CMD_SORT_DATE, 1 },
};

static const menu_item_t g_fm_options_items[] = {
	{ g_fm_confirm_label, FM_CMD_CONFIRM_TOGGLE, 1 },
};

static const menu_item_t g_fm_window_items[] = {
	{ "Cascata",     FM_CMD_CASCADE, 1 },
	{ "Lado a lado", FM_CMD_TILE,    1 },
};

static const menu_item_t g_fm_help_items[] = {
	{ "Sobre", FM_CMD_ABOUT, 1 },
};

#define FM_MENU_COUNT 6
static const menu_entry_t g_fm_menu_entries[FM_MENU_COUNT] = {
	{ "Arquivo", g_fm_file_items,    sizeof(g_fm_file_items) / sizeof(g_fm_file_items[0]) },
	{ "Arvore",  g_fm_tree_items,    sizeof(g_fm_tree_items) / sizeof(g_fm_tree_items[0]) },
	{ "Exibir",  g_fm_view_items,    sizeof(g_fm_view_items) / sizeof(g_fm_view_items[0]) },
	{ "Opcoes",  g_fm_options_items, sizeof(g_fm_options_items) / sizeof(g_fm_options_items[0]) },
	{ "Janela",  g_fm_window_items,  sizeof(g_fm_window_items) / sizeof(g_fm_window_items[0]) },
	{ "Ajuda",   g_fm_help_items,    sizeof(g_fm_help_items) / sizeof(g_fm_help_items[0]) },
};

// "Open" association table, generalizing v1's single hardcoded
// fm_open_in_editor call. Not a general "Open With"/type-registry system,
// and it can never launch an arbitrary program: there is no process/exec
// model in this kernel, so "opening" a file only ever means handing its id
// to one of the handful of statically-linked app_st vtables already
// compiled in. Unrecognized extensions fall back to Text Editor
// unconditionally, matching v1's behavior exactly.
typedef struct {
	const char *ext;
	const app_st *app;
	void (*set_target)(int file_id, int parent);
} fm_assoc_t;

static const fm_assoc_t g_fm_associations[] = {
	{ ".txt", &text_editor_app, text_editor_set_target },
};
#define FM_ASSOC_COUNT (sizeof(g_fm_associations) / sizeof(g_fm_associations[0]))

static void fm_status_copy(file_manager_state_t *s, const char *msg) {
	int i = 0;
	while (msg[i] && i < (int)sizeof(s->status) - 1) {
		s->status[i] = msg[i];
		i++;
	}
	s->status[i] = 0;
}

static void fm_append(char *dst, int *pos, int max, const char *src) {
	int i = 0;
	while (src[i] && *pos < max - 1) {
		dst[*pos] = src[i];
		(*pos)++;
		i++;
	}
	dst[*pos] = 0;
}

static void fm_update_default_status(file_manager_state_t *s) {
	char path[80];
	fs_path(s->cwd, path, sizeof(path));
	char free_buf[16];
	itoa(free_buf, fs_free_sectors());

	int pos = 0;
	fm_append(s->status, &pos, (int)sizeof(s->status), path);
	fm_append(s->status, &pos, (int)sizeof(s->status), "  |  livre: ");
	fm_append(s->status, &pos, (int)sizeof(s->status), free_buf);
	fm_append(s->status, &pos, (int)sizeof(s->status), " setores");
}

static void fm_update_confirm_label(file_manager_state_t *s) {
	const char *text = s->confirmations_enabled ? "Confirmar exclusao: Sim" : "Confirmar exclusao: Nao";
	int i = 0;
	while (text[i] && i < (int)sizeof(g_fm_confirm_label) - 1) {
		g_fm_confirm_label[i] = text[i];
		i++;
	}
	g_fm_confirm_label[i] = 0;
	(void)s;
}

static void fm_format_label(char *buf, int buf_max, const fs_dirent_t *e) {
	int pos = 0;
	if (e->type == FS_TYPE_FOLDER) {
		const char *prefix = "[D] ";
		for (int i = 0; prefix[i] && pos < buf_max - 1; i++) {
			buf[pos++] = prefix[i];
		}
	}
	for (int i = 0; e->name[i] && pos < buf_max - 1; i++) {
		buf[pos++] = e->name[i];
	}
	if (e->type == FS_TYPE_FILE) {
		char num[16];
		itoa(num, (int)e->size);
		const char *mid = "  (";
		for (int i = 0; mid[i] && pos < buf_max - 1; i++) {
			buf[pos++] = mid[i];
		}
		for (int i = 0; num[i] && pos < buf_max - 1; i++) {
			buf[pos++] = num[i];
		}
		if (pos < buf_max - 1) buf[pos++] = 'b';
		if (pos < buf_max - 1) buf[pos++] = ')';
	}
	buf[pos] = 0;
}

static int fm_ext_start(const char *name) {
	int len = 0, dot = -1;
	while (name[len]) {
		if (name[len] == '.') dot = len;
		len++;
	}
	return dot >= 0 ? dot : len;
}

static int fm_cmp(int sort_mode, int id_a, int id_b) {
	const fs_dirent_t *a = fs_get(id_a);
	const fs_dirent_t *b = fs_get(id_b);
	if (!a || !b) return 0;

	// Folders always sort before files, matching winfile's default grouping.
	if (a->type != b->type) {
		return (a->type == FS_TYPE_FOLDER) ? -1 : 1;
	}
	if (sort_mode == FM_SORT_SIZE && a->size != b->size) {
		return (a->size < b->size) ? -1 : 1;
	}
	if (sort_mode == FM_SORT_DATE && a->modified != b->modified) {
		return (a->modified < b->modified) ? -1 : 1;
	}
	if (sort_mode == FM_SORT_TYPE) {
		int c = strncmp(a->name + fm_ext_start(a->name), b->name + fm_ext_start(b->name), FS_NAME_MAX);
		if (c != 0) return c;
	}
	return strncmp(a->name, b->name, FS_NAME_MAX);
}

static void fm_sort_ids(int sort_mode, int *ids, int count) {
	for (int i = 1; i < count; i++) {
		int key = ids[i];
		int j = i - 1;
		while (j >= 0 && fm_cmp(sort_mode, ids[j], key) > 0) {
			ids[j + 1] = ids[j];
			j--;
		}
		ids[j + 1] = key;
	}
}

static void fm_refresh_list(file_manager_state_t *s) {
	int ids[LISTBOX_MAX_ROWS];
	int n = fs_list(s->cwd, ids, LISTBOX_MAX_ROWS);
	fm_sort_ids(s->sort_mode, ids, n);

	for (int i = 0; i < n; i++) {
		s->ids[i] = ids[i];
		const fs_dirent_t *e = fs_get(ids[i]);
		if (e) {
			fm_format_label(s->labels_buf[i], FM_LABEL_MAX, e);
		} else {
			s->labels_buf[i][0] = 0;
		}
	}
	s->count = n;
	s->list.count = n; // keep in sync now, not just at the next listbox_draw
	listbox_clear_selection(&s->list);
	s->list.top = 0;
	s->list.anchor_row = -1;

	fm_update_default_status(s);
}

static void fm_refresh_all(file_manager_state_t *s) {
	treeview_rebuild(&s->tree);
	s->tree.selected_id = s->cwd;
	fm_refresh_list(s);
}

static int fm_tree_row_for_id(treeview_st *tv, int id) {
	for (int i = 0; i < tv->row_count; i++) {
		if (tv->rows[i].fs_id == id) {
			return i;
		}
	}
	return -1;
}

static void fm_selected_ids(file_manager_state_t *s, int *out_ids, int *out_count) {
	int n = 0;
	for (int i = 0; i < s->count && n < LISTBOX_MAX_ROWS; i++) {
		if (s->list.selected[i]) {
			out_ids[n++] = s->ids[i];
		}
	}
	*out_count = n;
}

static void fm_delete_selected(file_manager_state_t *s) {
	int sel_ids[LISTBOX_MAX_ROWS], sel_count;
	fm_selected_ids(s, sel_ids, &sel_count);
	int failed = 0;
	for (int i = 0; i < sel_count; i++) {
		if (fs_delete(sel_ids[i]) != 0) {
			failed++;
		}
	}
	fm_refresh_all(s);
	if (failed > 0) {
		fm_status_copy(s, "alguns itens nao puderam ser excluidos (pasta nao vazia?)");
	}
}

static void fm_transfer_selected(file_manager_state_t *s, int dest_id, int is_copy) {
	int sel_ids[LISTBOX_MAX_ROWS], sel_count;
	fm_selected_ids(s, sel_ids, &sel_count);
	int failed = 0;
	for (int i = 0; i < sel_count; i++) {
		int r = is_copy ? fs_copy(sel_ids[i], dest_id, 0) : fs_move(sel_ids[i], dest_id);
		if (r < 0) {
			failed++;
		}
	}
	fm_refresh_all(s);
	if (failed > 0) {
		fm_status_copy(s, "alguns itens nao puderam ser movidos/copiados");
	}
}

static void fm_apply_confirm(file_manager_state_t *s, int result) {
	if (result == 1 && s->confirm.pending_action == FM_ACTION_DELETE) {
		fm_delete_selected(s);
	}
}

static void fm_commit_prompt(file_manager_state_t *s) {
	int ok = -1;
	if (s->prompt_kind == FM_PROMPT_FOLDER) {
		ok = fs_create_folder(s->cwd, s->prompt.text) >= 0 ? 0 : -1;
	} else if (s->prompt_kind == FM_PROMPT_FILE) {
		ok = fs_create_file(s->cwd, s->prompt.text) >= 0 ? 0 : -1;
	} else if (s->prompt_kind == FM_PROMPT_RENAME) {
		ok = fs_rename(s->rename_target_id, s->prompt.text);
	}
	s->prompt_kind = FM_PROMPT_NONE;
	lineedit_close(&s->prompt);
	fm_refresh_all(s);
	if (ok != 0) {
		fm_status_copy(s, "nome invalido ou repetido");
	}
}

static int fm_name_ends_with(const char *name, const char *suffix) {
	int nlen = 0;
	while (name[nlen]) nlen++;
	int slen = 0;
	while (suffix[slen]) slen++;
	if (slen > nlen) return 0;
	for (int i = 0; i < slen; i++) {
		if (name[nlen - slen + i] != suffix[i]) return 0;
	}
	return 1;
}

static void fm_open_file(file_manager_state_t *s, int file_id) {
	const fs_dirent_t *e = fs_get(file_id);
	const fm_assoc_t *assoc = 0;
	if (e) {
		for (unsigned int i = 0; i < FM_ASSOC_COUNT; i++) {
			if (fm_name_ends_with(e->name, g_fm_associations[i].ext)) {
				assoc = &g_fm_associations[i];
				break;
			}
		}
	}
	const app_st *app = assoc ? assoc->app : &text_editor_app;
	void (*set_target)(int, int) = assoc ? assoc->set_target : text_editor_set_target;
	set_target(file_id, s->cwd);
	rect_st r = { 120, 60, 460, 300 };
	if (!wm_create_window(app, app->name, r, TEXT_EDITOR_MIN_W, TEXT_EDITOR_MIN_H)) {
		fm_status_copy(s, "limite de janelas atingido");
	}
}

static void fm_open_selected_or_navigate(file_manager_state_t *s, int row) {
	if (row < 0 || row >= s->count) {
		return;
	}
	const fs_dirent_t *e = fs_get(s->ids[row]);
	if (!e) {
		return;
	}
	if (e->type == FS_TYPE_FOLDER) {
		s->cwd = s->ids[row];
		s->tree.selected_id = s->cwd;
		fm_refresh_list(s);
	} else {
		fm_open_file(s, s->ids[row]);
	}
}

static void fm_run_command(file_manager_state_t *s, int cmd) {
	switch (cmd) {
		case FM_CMD_NEW_FOLDER:
			s->prompt_kind = FM_PROMPT_FOLDER;
			lineedit_open(&s->prompt, "Nova pasta:");
			break;
		case FM_CMD_NEW_FILE:
			s->prompt_kind = FM_PROMPT_FILE;
			lineedit_open(&s->prompt, "Novo arquivo:");
			break;
		case FM_CMD_RENAME: {
			int sel_ids[LISTBOX_MAX_ROWS], sel_count;
			fm_selected_ids(s, sel_ids, &sel_count);
			if (sel_count != 1) {
				fm_status_copy(s, "selecione exatamente um item para renomear");
				break;
			}
			const fs_dirent_t *e = fs_get(sel_ids[0]);
			if (!e) break;
			s->rename_target_id = sel_ids[0];
			s->prompt_kind = FM_PROMPT_RENAME;
			lineedit_open(&s->prompt, "Renomear para:");
			int i = 0;
			while (e->name[i] && i < LINEEDIT_MAX - 1) {
				s->prompt.text[i] = e->name[i];
				i++;
			}
			s->prompt.text[i] = 0;
			s->prompt.len = i;
			break;
		}
		case FM_CMD_DELETE: {
			int sel_ids[LISTBOX_MAX_ROWS], sel_count;
			fm_selected_ids(s, sel_ids, &sel_count);
			if (sel_count == 0) {
				fm_status_copy(s, "nenhum item selecionado");
				break;
			}
			if (s->confirmations_enabled) {
				confirm_open(&s->confirm, "Excluir os itens selecionados?", FM_ACTION_DELETE);
			} else {
				fm_delete_selected(s);
			}
			break;
		}
		case FM_CMD_MOVE_TO:
		case FM_CMD_COPY_TO: {
			int sel_ids[LISTBOX_MAX_ROWS], sel_count;
			fm_selected_ids(s, sel_ids, &sel_count);
			if (sel_count == 0) {
				fm_status_copy(s, "nenhum item selecionado");
				break;
			}
			s->pending_action = (cmd == FM_CMD_MOVE_TO) ? FM_ACTION_MOVE_TO : FM_ACTION_COPY_TO;
			fm_status_copy(s, "clique na pasta destino na arvore");
			break;
		}
		case FM_CMD_TREE_TOGGLE: {
			int row = fm_tree_row_for_id(&s->tree, s->cwd);
			if (row >= 0) {
				treeview_toggle_expand(&s->tree, row);
			}
			break;
		}
		case FM_CMD_SORT_NAME: s->sort_mode = FM_SORT_NAME; fm_refresh_list(s); break;
		case FM_CMD_SORT_TYPE: s->sort_mode = FM_SORT_TYPE; fm_refresh_list(s); break;
		case FM_CMD_SORT_SIZE: s->sort_mode = FM_SORT_SIZE; fm_refresh_list(s); break;
		case FM_CMD_SORT_DATE: s->sort_mode = FM_SORT_DATE; fm_refresh_list(s); break;
		case FM_CMD_CONFIRM_TOGGLE:
			s->confirmations_enabled = !s->confirmations_enabled;
			fm_update_confirm_label(s);
			break;
		case FM_CMD_CASCADE:
			wm_cascade();
			break;
		case FM_CMD_TILE:
			wm_tile();
			break;
		case FM_CMD_ABOUT:
			fm_status_copy(s, "rSystemOS File Manager");
			break;
		default:
			break;
	}
}

static void fm_scrollbar_track_click(scrollbar_st *sb, int y) {
	int max_pos = sb->total - sb->visible;
	if (max_pos <= 0) {
		return;
	}
	int total = sb->total > 0 ? sb->total : 1;
	int mid = sb->rect.y + (sb->rect.h * sb->pos) / total;
	sb->pos += (y < mid) ? -sb->visible : sb->visible;
	if (sb->pos < 0) sb->pos = 0;
	if (sb->pos > max_pos) sb->pos = max_pos;
}

static void *file_manager_on_init(wm_window_st *win) {
	(void)win;
	file_manager_state_t *s = (file_manager_state_t *)kmalloc(sizeof(file_manager_state_t));

	s->cwd = FS_ROOT;
	s->split_x = 130;
	s->dragging_splitter = 0;
	s->count = 0;
	s->sort_mode = FM_SORT_NAME;
	s->prompt_kind = FM_PROMPT_NONE;
	s->rename_target_id = -1;
	s->pending_action = FM_ACTION_NONE;
	s->dragging = 0;
	s->confirmations_enabled = 1;
	s->last_click_row = -1;
	s->last_click_tick = 0;
	s->status[0] = 0;

	rect_st zero = { 0, 0, 0, 0 };
	menubar_init(&s->menubar, zero, g_fm_menu_entries, FM_MENU_COUNT);
	treeview_init(&s->tree, zero);
	listbox_init(&s->list, zero);
	s->tree_sb.rect = zero; s->tree_sb.total = 0; s->tree_sb.visible = 1; s->tree_sb.pos = 0; s->tree_sb.dragging = 0;
	s->list_sb.rect = zero; s->list_sb.total = 0; s->list_sb.visible = 1; s->list_sb.pos = 0; s->list_sb.dragging = 0;
	s->splitter_rect = zero;
	s->confirm.active = 0;
	lineedit_close(&s->prompt);

	fm_update_confirm_label(s);
	fm_refresh_all(s);
	return s;
}

static void file_manager_on_show(wm_window_st *win, void *state, rect_st content) {
	(void)win;
	file_manager_state_t *s = (file_manager_state_t *)state;
	fill_rect(content.x, content.y, content.w, content.h, GUI_COLOR_BG);

	rect_st menubar_rect = { content.x, content.y, content.w, MENUBAR_HEIGHT };
	s->menubar.rect = menubar_rect;
	menubar_draw_bar(&s->menubar); // dropdown drawn last, see bottom of this function

	rect_st drivebar_rect = { content.x, content.y + MENUBAR_HEIGHT, content.w, FM_DRIVEBAR_H };
	fill_rect(drivebar_rect.x, drivebar_rect.y, drivebar_rect.w, drivebar_rect.h, GUI_COLOR_BG);
	{
		// Single, always-selected "C:" -- this kernel boots from and
		// addresses exactly one IDE-backed disk, so a real drive-letter
		// picker would have nothing behind it to switch to.
		widget_st drive_btn;
		drive_btn.rect.x = drivebar_rect.x + FM_PAD;
		drive_btn.rect.y = drivebar_rect.y + 2;
		drive_btn.rect.w = 40;
		drive_btn.rect.h = drivebar_rect.h - 4;
		drive_btn.label = "C:";
		drive_btn.selected = 1;
		drive_btn.pressed = 1;
		widget_button_draw(&drive_btn);
	}

	int pane_y = drivebar_rect.y + drivebar_rect.h;
	int pane_h = content.h - MENUBAR_HEIGHT - FM_DRIVEBAR_H - FM_STATUS_H;
	if (pane_h < 20) pane_h = 20;
	rect_st pane_area = { content.x, pane_y, content.w, pane_h };

	int min_tree = 60 + FM_SCROLLBAR_W;
	int min_list = 80 + FM_SCROLLBAR_W;
	if (s->split_x < min_tree) {
		s->split_x = min_tree;
	}
	if (s->split_x > pane_area.w - SPLITTER_WIDTH - min_list) {
		s->split_x = pane_area.w - SPLITTER_WIDTH - min_list;
	}
	if (s->split_x < min_tree) {
		s->split_x = min_tree; // window too narrow overall -- best effort
	}

	rect_st tree_rect = { pane_area.x, pane_area.y, s->split_x - FM_SCROLLBAR_W, pane_area.h };
	rect_st tree_sb_rect = { pane_area.x + s->split_x - FM_SCROLLBAR_W, pane_area.y, FM_SCROLLBAR_W, pane_area.h };
	rect_st splitter_rect = splitter_bar_rect(pane_area, s->split_x);
	int list_x = pane_area.x + s->split_x + SPLITTER_WIDTH;
	rect_st list_rect = { list_x, pane_area.y, pane_area.x + pane_area.w - list_x - FM_SCROLLBAR_W, pane_area.h };
	rect_st list_sb_rect = { list_x + list_rect.w, pane_area.y, FM_SCROLLBAR_W, pane_area.h };

	s->tree.rect = tree_rect;
	s->tree_sb.rect = tree_sb_rect;
	s->tree_sb.total = s->tree.row_count;
	s->tree_sb.visible = tree_rect.h / FONT_H;
	if (s->tree_sb.visible < 1) s->tree_sb.visible = 1;
	s->tree_sb.pos = s->tree.top;

	s->list.rect = list_rect;
	s->list_sb.rect = list_sb_rect;
	s->list_sb.total = s->list.count;
	s->list_sb.visible = list_rect.h / FONT_H;
	if (s->list_sb.visible < 1) s->list_sb.visible = 1;
	s->list_sb.pos = s->list.top;

	s->splitter_rect = splitter_rect;

	treeview_draw(&s->tree);
	scrollbar_draw(&s->tree_sb);
	splitter_draw(splitter_rect);

	const char *label_ptrs[LISTBOX_MAX_ROWS];
	for (int i = 0; i < s->count; i++) {
		label_ptrs[i] = s->labels_buf[i];
	}
	listbox_draw(&s->list, label_ptrs, s->count);
	scrollbar_draw(&s->list_sb);

	rect_st status_rect = { content.x, content.y + content.h - FM_STATUS_H, content.w, FM_STATUS_H };
	fill_rect(status_rect.x, status_rect.y, status_rect.w, status_rect.h, GUI_COLOR_BG);
	draw_text(status_rect.x + FM_PAD, status_rect.y + (FM_STATUS_H - FONT_H) / 2, s->status, GUI_COLOR_BLACK);

	// Dropdown last: it extends below the bar, over the drive bar/pane/
	// status content just drawn above, and would get painted over
	// otherwise (see menubar.h's split-draw note).
	menubar_draw_dropdown(&s->menubar);

	if (s->prompt.active) {
		lineedit_draw(&s->prompt, content);
	}
	if (s->confirm.active) {
		confirm_draw(&s->confirm, content);
	}
}

static void file_manager_on_key_down(wm_window_st *win, void *state, int ascii, int mods) {
	(void)win;
	(void)mods;
	file_manager_state_t *s = (file_manager_state_t *)state;

	if (s->confirm.active) {
		int r = confirm_key_down(&s->confirm, ascii);
		if (r != 0) {
			fm_apply_confirm(s, r);
		}
		return;
	}
	if (!s->prompt.active) {
		return; // this app is mouse/menu-driven otherwise
	}
	int r = lineedit_key(&s->prompt, ascii);
	if (r != 1) {
		return;
	}
	fm_commit_prompt(s);
}

// Every rect this app stores (menubar/tree/list/scrollbars/splitter) is
// filled in during on_show using the ABSOLUTE content rect the WM hands
// it, because the primitives' own _draw() functions need absolute screen
// coordinates for fill_rect/draw_text. But app_st's mouse callbacks
// deliver lx/ly CONTENT-RELATIVE (0,0 = top-left of content) -- so hit
// testing against those stored rects needs them converted back to
// absolute first, via window_content_rect(&win->base).
static void fm_to_absolute(wm_window_st *win, int lx, int ly, int *ax, int *ay) {
	rect_st content = window_content_rect(&win->base);
	*ax = content.x + lx;
	*ay = content.y + ly;
}

static void file_manager_on_mouse_down(wm_window_st *win, void *state, int lx, int ly, int buttons) {
	(void)buttons;
	file_manager_state_t *s = (file_manager_state_t *)state;
	int ax, ay;
	fm_to_absolute(win, lx, ly, &ax, &ay);

	if (s->confirm.active) {
		rect_st content = window_content_rect(&win->base);
		int r = confirm_mouse_down(&s->confirm, content, ax, ay);
		if (r != 0) {
			fm_apply_confirm(s, r);
		}
		return;
	}
	if (s->prompt.active) {
		return; // naming is keyboard-only, matching v1
	}

	if (s->menubar.open_index >= 0) {
		int cmd = menubar_hit_item(&s->menubar, ax, ay);
		if (cmd >= 0) {
			menubar_close(&s->menubar);
			if (cmd == FM_CMD_EXIT) {
				wm_close_window(win);
				return;
			}
			fm_run_command(s, cmd);
			return;
		}
		int prev = s->menubar.open_index;
		int top = menubar_hit_top(&s->menubar, ax, ay);
		menubar_close(&s->menubar);
		if (top >= 0 && top != prev) {
			menubar_open(&s->menubar, top);
		}
		return;
	}

	int top = menubar_hit_top(&s->menubar, ax, ay);
	if (top >= 0) {
		menubar_open(&s->menubar, top);
		return;
	}

	if (splitter_hit_test(s->splitter_rect, ax, ay)) {
		s->dragging_splitter = 1;
		return;
	}

	int tree_sb_hit = scrollbar_hit_test(&s->tree_sb, ax, ay);
	if (tree_sb_hit >= 0) {
		if (tree_sb_hit == 1) {
			s->tree_sb.dragging = 1;
		} else {
			fm_scrollbar_track_click(&s->tree_sb, ay);
			s->tree.top = s->tree_sb.pos;
		}
		return;
	}

	int list_sb_hit = scrollbar_hit_test(&s->list_sb, ax, ay);
	if (list_sb_hit >= 0) {
		if (list_sb_hit == 1) {
			s->list_sb.dragging = 1;
		} else {
			fm_scrollbar_track_click(&s->list_sb, ay);
			s->list.top = s->list_sb.pos;
		}
		return;
	}

	int tree_row = treeview_hit_row(&s->tree, ax, ay);
	if (tree_row >= 0) {
		if (treeview_hit_toggle(&s->tree, tree_row, ax, ay)) {
			treeview_toggle_expand(&s->tree, tree_row);
			return;
		}
		int target_id = s->tree.rows[tree_row].fs_id;
		if (s->pending_action != FM_ACTION_NONE) {
			int is_copy = (s->pending_action == FM_ACTION_COPY_TO);
			s->pending_action = FM_ACTION_NONE;
			fm_transfer_selected(s, target_id, is_copy);
			return;
		}
		s->cwd = target_id;
		s->tree.selected_id = target_id;
		fm_refresh_list(s);
		return;
	}

	int row = listbox_row_at(&s->list, ax, ay);
	if (row >= 0) {
		unsigned int now = timer_get_ticks();
		if (s->last_click_row == row && (now - s->last_click_tick) < FM_DOUBLE_CLICK_TICKS) {
			s->last_click_row = -1;
			fm_open_selected_or_navigate(s, row);
			return;
		}
		unsigned int mods = keyboard_get_mods();
		if (mods & KEY_MOD_SHIFT) {
			listbox_range_select(&s->list, row);
		} else if (mods & KEY_MOD_CTRL) {
			listbox_toggle(&s->list, row);
		} else {
			listbox_select_single(&s->list, row);
		}
		s->last_click_row = row;
		s->last_click_tick = now;
		s->dragging = 1; // tentative; only acts if released over a different tree folder
		return;
	}
}

static void file_manager_on_mouse_move(wm_window_st *win, void *state, int lx, int ly) {
	file_manager_state_t *s = (file_manager_state_t *)state;
	int ax, ay;
	fm_to_absolute(win, lx, ly, &ax, &ay);

	if (s->dragging_splitter) {
		s->split_x = ax - s->tree.rect.x; // tree.rect.x == pane_area.x
		return;
	}
	if (s->tree_sb.dragging) {
		scrollbar_drag_to(&s->tree_sb, ay);
		s->tree.top = s->tree_sb.pos;
		return;
	}
	if (s->list_sb.dragging) {
		scrollbar_drag_to(&s->list_sb, ay);
		s->list.top = s->list_sb.pos;
		return;
	}
	if (s->menubar.open_index >= 0) {
		menubar_track_hover(&s->menubar, ax, ay);
		return;
	}
	// s->dragging (list -> tree drag-and-drop): no ghost cursor rendering,
	// the drop is simply resolved at mouse-up.
}

static void file_manager_on_mouse_up(wm_window_st *win, void *state, int lx, int ly, int buttons) {
	(void)buttons;
	file_manager_state_t *s = (file_manager_state_t *)state;
	int ax, ay;
	fm_to_absolute(win, lx, ly, &ax, &ay);

	s->dragging_splitter = 0;
	s->tree_sb.dragging = 0;
	s->list_sb.dragging = 0;

	if (s->dragging) {
		s->dragging = 0;
		int row = treeview_hit_row(&s->tree, ax, ay);
		if (row >= 0 && !treeview_hit_toggle(&s->tree, row, ax, ay)) {
			int dest = s->tree.rows[row].fs_id;
			if (dest != s->cwd) {
				unsigned int mods = keyboard_get_mods();
				fm_transfer_selected(s, dest, (mods & KEY_MOD_CTRL) != 0);
			}
		}
	}
}

static void file_manager_on_blur(wm_window_st *win, void *state) {
	(void)win;
	file_manager_state_t *s = (file_manager_state_t *)state;
	menubar_close(&s->menubar);
}

static void file_manager_on_close(wm_window_st *win, void *state) {
	(void)win;
	kfree(state);
}

const app_st file_manager_app = {
	.name = "File Manager",
	.icon = ICON_FILE_MANAGER,
	.on_init = file_manager_on_init,
	.on_show = file_manager_on_show,
	.on_key_down = file_manager_on_key_down,
	.on_mouse_down = file_manager_on_mouse_down,
	.on_mouse_up = file_manager_on_mouse_up,
	.on_mouse_move = file_manager_on_mouse_move,
	.on_tick = 0,
	.on_close = file_manager_on_close,
	.on_blur = file_manager_on_blur,
};
