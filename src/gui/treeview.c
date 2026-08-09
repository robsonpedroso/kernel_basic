#include "../include/treeview.h"
#include "../include/fs.h"
#include "../include/video.h"
#include "../include/gui.h"

typedef char treeview_row_count_check[(TREEVIEW_MAX_ROWS == FS_MAX_ENTRIES) ? 1 : -1];

#define TREEVIEW_INDENT   12
#define TREEVIEW_TOGGLE_W 10

void treeview_init(treeview_st *tv, rect_st rect) {
	tv->rect = rect;
	tv->row_count = 0;
	tv->top = 0;
	tv->selected_id = FS_ROOT;
	for (int i = 0; i < TREEVIEW_MAX_ROWS; i++) {
		tv->expanded[i] = 0;
	}
}

static int treeview_has_child_folder(int id) {
	int ids[FS_MAX_ENTRIES];
	int n = fs_list(id, ids, FS_MAX_ENTRIES);
	for (int i = 0; i < n; i++) {
		const fs_dirent_t *e = fs_get(ids[i]);
		if (e && e->type == FS_TYPE_FOLDER) {
			return 1;
		}
	}
	return 0;
}

static void treeview_walk(treeview_st *tv, int parent, int depth) {
	int ids[FS_MAX_ENTRIES];
	int n = fs_list(parent, ids, FS_MAX_ENTRIES);
	for (int i = 0; i < n; i++) {
		const fs_dirent_t *e = fs_get(ids[i]);
		if (!e || e->type != FS_TYPE_FOLDER) {
			continue;
		}
		if (tv->row_count >= TREEVIEW_MAX_ROWS) {
			return;
		}
		treeview_row_t *row = &tv->rows[tv->row_count++];
		row->fs_id = ids[i];
		row->depth = depth;
		row->has_children = treeview_has_child_folder(ids[i]);
		if (row->has_children && tv->expanded[ids[i]]) {
			treeview_walk(tv, ids[i], depth + 1);
		}
	}
}

void treeview_rebuild(treeview_st *tv) {
	tv->row_count = 0;
	if (tv->row_count < TREEVIEW_MAX_ROWS) {
		treeview_row_t *root = &tv->rows[tv->row_count++];
		root->fs_id = FS_ROOT;
		root->depth = 0;
		root->has_children = treeview_has_child_folder(FS_ROOT);
	}
	treeview_walk(tv, FS_ROOT, 1);
}

void treeview_draw(treeview_st *tv) {
	fill_rect(tv->rect.x, tv->rect.y, tv->rect.w, tv->rect.h, GUI_COLOR_LIGHT);

	int visible = tv->rect.h / FONT_H;
	if (visible <= 0) {
		visible = 1;
	}

	for (int i = 0; i < visible; i++) {
		int idx = tv->top + i;
		if (idx >= tv->row_count) {
			break;
		}
		treeview_row_t *row = &tv->rows[idx];
		int ry = tv->rect.y + i * FONT_H;
		int tx = tv->rect.x + row->depth * TREEVIEW_INDENT;

		unsigned char color = GUI_COLOR_BLACK;
		if (row->fs_id == tv->selected_id) {
			fill_rect(tv->rect.x, ry, tv->rect.w, FONT_H, GUI_COLOR_TITLE);
			color = GUI_COLOR_TITLE_TEXT;
		}

		if (row->has_children) {
			int expanded = (row->fs_id == FS_ROOT) ? 1 : tv->expanded[row->fs_id];
			draw_text(tx, ry, expanded ? "-" : "+", color);
		}
		const char *name = (row->fs_id == FS_ROOT) ? "C:\\" : fs_get(row->fs_id)->name;
		draw_text(tx + TREEVIEW_TOGGLE_W, ry, (char *)name, color);
	}
}

int treeview_hit_row(treeview_st *tv, int x, int y) {
	if (!rect_contains_point(&tv->rect, x, y)) {
		return -1;
	}
	int rel = (y - tv->rect.y) / FONT_H;
	int idx = tv->top + rel;
	if (idx < 0 || idx >= tv->row_count) {
		return -1;
	}
	return idx;
}

int treeview_hit_toggle(treeview_st *tv, int row, int x, int y) {
	(void)y; // the row was already resolved via treeview_hit_row
	if (row < 0 || row >= tv->row_count || !tv->rows[row].has_children) {
		return 0;
	}
	int tx = tv->rect.x + tv->rows[row].depth * TREEVIEW_INDENT;
	return x >= tx && x < tx + TREEVIEW_TOGGLE_W;
}

void treeview_toggle_expand(treeview_st *tv, int row) {
	if (row < 0 || row >= tv->row_count) {
		return;
	}
	int id = tv->rows[row].fs_id;
	if (id == FS_ROOT) {
		return; // root can't be collapsed in this simplified model
	}
	tv->expanded[id] = (unsigned char)!tv->expanded[id];
	treeview_rebuild(tv);
}
