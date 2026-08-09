#include "../../include/apps/terminal.h"
#include "../../include/wm.h"
#include "../../include/window.h"
#include "../../include/textbox.h"
#include "../../include/video.h"
#include "../../include/gui.h"
#include "../../include/heap.h"
#include "../../include/string.h"
#include "../../include/stdlib.h"
#include "../../include/timer.h"
#include "../../include/keyboard.h"
#include "../../include/fs.h"
#include "../../include/rtc.h"
#include "../../include/cc.h"

#define TERMINAL_COLS 40
#define TERMINAL_ROWS 23
#define TERMINAL_PAD 4
#define TERMINAL_BLINK_TICKS 50   // ~500ms @ 100Hz
#define TERMINAL_MAX_LINE 160     // DOS's own COMBUFLEN was 128; this fs allows
                                   // long path segments (FS_NAME_MAX=100), so 160
                                   // is a floor-not-ceiling widen of the old 64.
#define TERMINAL_MAX_ARGS 8
#define TERMINAL_HISTORY_N 16

typedef struct {
	textbox_st tb;
	char input_line[TERMINAL_MAX_LINE];
	int input_len;
	int cursor;      // edit-cursor offset into input_line
	int drawn_len;   // how many input cells were painted on the last redraw
	                 // (lets a shrinking edit blank out leftover cells)
	int prompt_row, prompt_col; // textbox position right after the prompt
	                             // string -- the input redraw is anchored here
	int blink_on;
	unsigned int last_blink_tick;
	int cwd; // FS_ROOT or a folder id -- per-window, mirrors file_manager.c's cwd
	char history[TERMINAL_HISTORY_N][TERMINAL_MAX_LINE];
	int history_count, history_head, history_pos; // history_pos == -1: not browsing
} terminal_state_t;

typedef int (*terminal_cmd_fn)(wm_window_st *win, terminal_state_t *s, int argc, char **argv);
typedef struct { const char *name; terminal_cmd_fn fn; const char *help; } terminal_cmd_t;

// Forward declared (incomplete array) so cmd_help() and terminal_exec() can
// reference it by name -- the full definition (with a {0,0,0} sentinel) sits
// at the bottom of the file, after every cmd_* handler it points to.
static const terminal_cmd_t g_commands[];

static void tp(terminal_state_t *s, const char *msg) {
	textbox_puts(&s->tb, msg);
	textbox_putc(&s->tb, '\n');
}

// ---- date/time formatting (rtc.h's packed FAT-style bits) ----

static void terminal_unpack_dt(unsigned int packed, int *y, int *mo, int *d, int *h, int *mi) {
	*y = 1980 + (int)((packed >> 25) & 0x7F);
	*mo = (int)((packed >> 21) & 0x0F);
	*d = (int)((packed >> 16) & 0x1F);
	*h = (int)((packed >> 11) & 0x1F);
	*mi = (int)((packed >> 5) & 0x3F);
}

static void terminal_put2(textbox_st *tb, int v) {
	if (v < 10) {
		textbox_putc(tb, '0');
	}
	char buf[8];
	itoa(buf, v);
	textbox_puts(tb, buf);
}

static void terminal_put_date(textbox_st *tb, unsigned int packed) {
	int y, mo, d, h, mi;
	terminal_unpack_dt(packed, &y, &mo, &d, &h, &mi);
	terminal_put2(tb, d);
	textbox_putc(tb, '/');
	terminal_put2(tb, mo);
	textbox_putc(tb, '/');
	char yb[8];
	itoa(yb, y);
	textbox_puts(tb, yb);
}

static void terminal_put_time(textbox_st *tb, unsigned int packed) {
	int y, mo, d, h, mi;
	terminal_unpack_dt(packed, &y, &mo, &d, &h, &mi);
	(void)y; (void)mo; (void)d;
	terminal_put2(tb, h);
	textbox_putc(tb, ':');
	terminal_put2(tb, mi);
}

// ---- line editing (single-line subset of editbuf.c's editbuf_key) ----

static void ti_insert(terminal_state_t *s, char c) {
	if (s->input_len >= TERMINAL_MAX_LINE - 1) {
		return;
	}
	for (int i = s->input_len; i > s->cursor; i--) {
		s->input_line[i] = s->input_line[i - 1];
	}
	s->input_line[s->cursor] = c;
	s->cursor++;
	s->input_len++;
}

static void ti_erase(terminal_state_t *s, int at) {
	for (int i = at; i < s->input_len - 1; i++) {
		s->input_line[i] = s->input_line[i + 1];
	}
	s->input_len--;
}

// Returns 1 if the input line changed and needs a redraw.
static int terminal_input_key(terminal_state_t *s, int ascii) {
	switch (ascii) {
		case KEY_LEFT:
			if (s->cursor > 0) s->cursor--;
			return 1;
		case KEY_RIGHT:
			if (s->cursor < s->input_len) s->cursor++;
			return 1;
		case KEY_HOME:
			s->cursor = 0;
			return 1;
		case KEY_END:
			s->cursor = s->input_len;
			return 1;
		case KEY_DELETE:
			if (s->cursor < s->input_len) {
				ti_erase(s, s->cursor);
				return 1;
			}
			return 0;
		case '\b':
			if (s->cursor > 0) {
				s->cursor--;
				ti_erase(s, s->cursor);
				return 1;
			}
			return 0;
		default:
			if (ascii >= 32 && ascii < 127) {
				ti_insert(s, (char)ascii);
				return 1;
			}
			return 0;
	}
}

// Repaints only the input region (from prompt_row/prompt_col onward) --
// unlike editbuf_render, the terminal's textbox also holds scrollback above
// the prompt that must never be touched here.
static void terminal_redraw_input(terminal_state_t *s) {
	textbox_st *tb = &s->tb;
	int n = s->input_len > s->drawn_len ? s->input_len : s->drawn_len;
	for (int i = 0; i < n; i++) {
		int abs_col = s->prompt_col + i;
		int row = s->prompt_row + abs_col / tb->cols;
		int col = abs_col % tb->cols;
		if (row >= tb->rows) {
			break; // input wrapped past the bottom row -- same limitation
			       // real DOS has at the bottom of an 80x25 screen
		}
		tb->cells[row * tb->stride + col] = (i < s->input_len) ? s->input_line[i] : ' ';
	}
	s->drawn_len = s->input_len;

	int abs_col = s->prompt_col + s->cursor;
	int row = s->prompt_row + abs_col / tb->cols;
	int col = abs_col % tb->cols;
	if (row >= tb->rows) {
		row = tb->rows - 1;
		col = tb->cols - 1;
	}
	tb->cursor_row = row;
	tb->cursor_col = col;
}

// Prints "C:<path>>" and records where the input region starts. Also resets
// the per-line edit state, so this doubles as "start a fresh input line".
static void terminal_print_prompt(terminal_state_t *s) {
	char path[80];
	fs_path(s->cwd, path, sizeof(path));
	textbox_puts(&s->tb, "C:");
	textbox_puts(&s->tb, path);
	textbox_puts(&s->tb, ">");
	s->prompt_row = s->tb.cursor_row;
	s->prompt_col = s->tb.cursor_col;
	s->input_len = 0;
	s->cursor = 0;
	s->drawn_len = 0;
}

// ---- command history ----

static void terminal_history_push(terminal_state_t *s) {
	if (s->input_len == 0) {
		return;
	}
	int len = s->input_len < TERMINAL_MAX_LINE - 1 ? s->input_len : TERMINAL_MAX_LINE - 1;
	for (int i = 0; i < len; i++) {
		s->history[s->history_head][i] = s->input_line[i];
	}
	s->history[s->history_head][len] = 0;
	s->history_head = (s->history_head + 1) % TERMINAL_HISTORY_N;
	if (s->history_count < TERMINAL_HISTORY_N) {
		s->history_count++;
	}
	s->history_pos = -1;
}

// back = 0 is the most recently entered line, 1 the one before that, etc.
static void terminal_history_load(terminal_state_t *s, int back) {
	int idx = (s->history_head - 1 - back + TERMINAL_HISTORY_N * 2) % TERMINAL_HISTORY_N;
	int len = strlen(s->history[idx]);
	for (int i = 0; i < len; i++) {
		s->input_line[i] = s->history[idx][i];
	}
	s->input_len = len;
	s->cursor = len;
}

// ---- argv tokenizer (splits on spaces, mutates line in place) ----

static int terminal_tokenize(char *line, char **argv, int max_argv) {
	int argc = 0;
	int i = 0;
	while (line[i]) {
		while (line[i] == ' ') i++;
		if (!line[i]) break;
		if (argc < max_argv) argv[argc++] = &line[i];
		while (line[i] && line[i] != ' ') i++;
		if (line[i] == ' ') {
			line[i] = 0;
			i++;
		}
	}
	return argc;
}

// ---- dir sorting (folders first, then name -- same grouping file_manager.c
// uses) ----

static int terminal_entry_cmp(int id_a, int id_b) {
	const fs_dirent_t *a = fs_get(id_a);
	const fs_dirent_t *b = fs_get(id_b);
	if (!a || !b) return 0;
	if (a->type != b->type) {
		return (a->type == FS_TYPE_FOLDER) ? -1 : 1;
	}
	return strncmp(a->name, b->name, FS_NAME_MAX);
}

static void terminal_sort_ids(int *ids, int count) {
	for (int i = 1; i < count; i++) {
		int key = ids[i];
		int j = i - 1;
		while (j >= 0 && terminal_entry_cmp(ids[j], key) > 0) {
			ids[j + 1] = ids[j];
			j--;
		}
		ids[j + 1] = key;
	}
}

// Is `id` a valid, resolved directory (FS_ROOT counts -- it has no table entry)?
static int terminal_is_dir(int id) {
	if (id == FS_ROOT) return 1;
	const fs_dirent_t *e = fs_get(id);
	return e && e->type == FS_TYPE_FOLDER;
}

// ---- commands ----

static int cmd_help(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win; (void)argc; (void)argv;
	textbox_puts(&s->tb, "comandos disponiveis:\n");
	for (int i = 0; g_commands[i].name; i++) {
		textbox_puts(&s->tb, g_commands[i].name);
		textbox_puts(&s->tb, " - ");
		textbox_puts(&s->tb, g_commands[i].help);
		textbox_putc(&s->tb, '\n');
	}
	return 0;
}

static int cmd_clear(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win; (void)argc; (void)argv;
	textbox_clear(&s->tb);
	return 0;
}

static int cmd_echo(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	for (int i = 1; i < argc; i++) {
		if (i > 1) textbox_putc(&s->tb, ' ');
		textbox_puts(&s->tb, argv[i]);
	}
	textbox_putc(&s->tb, '\n');
	return 0;
}

static int cmd_info(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win; (void)argc; (void)argv;
	textbox_puts(&s->tb, "rSystemOS terminal\n");
	return 0;
}

static int cmd_ver(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win; (void)argc; (void)argv;
	textbox_puts(&s->tb, "rSystemOS [Versao 0.1]\n");
	return 0;
}

static int cmd_vol(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win; (void)argc; (void)argv;
	textbox_puts(&s->tb, "Unidade C: sem rotulo de volume\n");
	char num[16];
	itoa(num, fs_free_sectors() * 512);
	textbox_puts(&s->tb, num);
	textbox_puts(&s->tb, " bytes livres\n");
	return 0;
}

static int cmd_date(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win; (void)argc; (void)argv;
	textbox_puts(&s->tb, "Data atual: ");
	terminal_put_date(&s->tb, rtc_now());
	textbox_putc(&s->tb, '\n');
	return 0;
}

static int cmd_time(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win; (void)argc; (void)argv;
	textbox_puts(&s->tb, "Hora atual: ");
	terminal_put_time(&s->tb, rtc_now());
	textbox_putc(&s->tb, '\n');
	return 0;
}

static int cmd_exit(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)s; (void)argc; (void)argv;
	wm_close_window(win);
	return 1;
}

static int cmd_dir(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	int target = s->cwd;
	if (argc > 1) {
		target = fs_resolve_path(s->cwd, argv[1]);
		if (!terminal_is_dir(target)) {
			tp(s, "diretorio nao encontrado");
			return 0;
		}
	}

	char path[80];
	fs_path(target, path, sizeof(path));
	textbox_puts(&s->tb, "Diretorio de C:");
	textbox_puts(&s->tb, path);
	textbox_puts(&s->tb, "\n\n");

	int ids[FS_MAX_ENTRIES];
	int n = fs_list(target, ids, FS_MAX_ENTRIES);
	terminal_sort_ids(ids, n);

	for (int i = 0; i < n; i++) {
		const fs_dirent_t *e = fs_get(ids[i]);
		if (!e) continue;
		terminal_put_date(&s->tb, e->modified);
		textbox_putc(&s->tb, ' ');
		terminal_put_time(&s->tb, e->modified);
		textbox_puts(&s->tb, "  ");
		if (e->type == FS_TYPE_FOLDER) {
			textbox_puts(&s->tb, "<DIR>");
		} else {
			char num[16];
			itoa(num, (int)e->size);
			textbox_puts(&s->tb, num);
		}
		textbox_puts(&s->tb, "  ");
		textbox_puts(&s->tb, e->name);
		textbox_putc(&s->tb, '\n');
	}

	char cnt[16], freeb[16];
	itoa(cnt, n);
	itoa(freeb, fs_free_sectors() * 512);
	textbox_putc(&s->tb, '\n');
	textbox_puts(&s->tb, cnt);
	textbox_puts(&s->tb, " arquivo(s)   ");
	textbox_puts(&s->tb, freeb);
	textbox_puts(&s->tb, " bytes livres\n");
	return 0;
}

static int cmd_cd(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	if (argc < 2) {
		char path[80];
		fs_path(s->cwd, path, sizeof(path));
		textbox_puts(&s->tb, "C:");
		textbox_puts(&s->tb, path);
		textbox_putc(&s->tb, '\n');
		return 0;
	}
	int target = fs_resolve_path(s->cwd, argv[1]);
	if (target == FS_NOT_FOUND) {
		tp(s, "diretorio nao encontrado");
		return 0;
	}
	if (!terminal_is_dir(target)) {
		tp(s, "nao e um diretorio");
		return 0;
	}
	s->cwd = target;
	return 0;
}

static int cmd_md(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	if (argc < 2) {
		tp(s, "uso: md <nome>");
		return 0;
	}
	if (fs_create_folder(s->cwd, argv[1]) < 0) {
		tp(s, "nao foi possivel criar diretorio");
	}
	return 0;
}

static int cmd_rd(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	if (argc < 2) {
		tp(s, "uso: rd <nome>");
		return 0;
	}
	int id = fs_resolve_path(s->cwd, argv[1]);
	if (id < 0 || id == FS_ROOT || !fs_get(id) || fs_get(id)->type != FS_TYPE_FOLDER) {
		tp(s, "diretorio nao encontrado");
		return 0;
	}
	if (fs_delete(id) < 0) {
		tp(s, "diretorio nao vazio");
	}
	return 0;
}

static int cmd_del(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	if (argc < 2) {
		tp(s, "uso: del <nome>");
		return 0;
	}
	int id = fs_resolve_path(s->cwd, argv[1]);
	if (id < 0 || id == FS_ROOT || !fs_get(id) || fs_get(id)->type != FS_TYPE_FILE) {
		tp(s, "arquivo nao encontrado");
		return 0;
	}
	fs_delete(id);
	return 0;
}

static int cmd_ren(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	if (argc < 3) {
		tp(s, "uso: ren <nome_atual> <novo_nome>");
		return 0;
	}
	int id = fs_resolve_path(s->cwd, argv[1]);
	if (id < 0 || id == FS_ROOT) {
		tp(s, "arquivo nao encontrado");
		return 0;
	}
	if (fs_rename(id, argv[2]) < 0) {
		tp(s, "ja existe um arquivo com esse nome");
	}
	return 0;
}

static int cmd_copy(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	if (argc < 3) {
		tp(s, "uso: copy <origem> <destino>");
		return 0;
	}
	int src = fs_resolve_path(s->cwd, argv[1]);
	if (src < 0 || src == FS_ROOT || !fs_get(src) || fs_get(src)->type != FS_TYPE_FILE) {
		tp(s, "arquivo nao encontrado");
		return 0;
	}
	int dst_dir = fs_resolve_path(s->cwd, argv[2]);
	int result;
	if (terminal_is_dir(dst_dir)) {
		result = fs_copy(src, dst_dir, 0);
	} else {
		result = fs_copy(src, s->cwd, argv[2]);
	}
	if (result < 0) {
		tp(s, "nao foi possivel copiar (espaco insuficiente ou nome ja existe)");
	}
	return 0;
}

// Static, not on-stack: FS_MAX_FILE_SIZE (4096) bytes, same reasoning as
// fs.c's own g_copy_buf scratch buffer.
static char g_terminal_type_buf[FS_MAX_FILE_SIZE + 1];

static int cmd_type(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	if (argc < 2) {
		tp(s, "uso: type <nome>");
		return 0;
	}
	int id = fs_resolve_path(s->cwd, argv[1]);
	if (id < 0 || id == FS_ROOT || !fs_get(id) || fs_get(id)->type != FS_TYPE_FILE) {
		tp(s, "arquivo nao encontrado");
		return 0;
	}
	int n = fs_read_file(id, g_terminal_type_buf, FS_MAX_FILE_SIZE);
	if (n < 0) n = 0;
	g_terminal_type_buf[n] = 0;
	textbox_puts(&s->tb, g_terminal_type_buf);
	if (n == 0 || g_terminal_type_buf[n - 1] != '\n') {
		textbox_putc(&s->tb, '\n');
	}
	return 0;
}

// Set right before calling a cc-compiled program and cleared right after
// (single-threaded/cooperative kernel, no locking needed) -- same static
// -global handoff pattern text_editor.c's text_editor_set_target() uses
// to cross an unrelated callback boundary. cc.c is terminal-agnostic (it
// only knows a `void (*)(int)` function pointer), so this is where the
// print target actually lives.
static terminal_state_t *g_cc_print_target;

static void cc_builtin_print(int v) {
	if (!g_cc_print_target) {
		return;
	}
	char buf[16];
	itoa(buf, v);
	textbox_puts(&g_cc_print_target->tb, buf);
	textbox_putc(&g_cc_print_target->tb, '\n');
}

// Static, not on-stack: same reasoning as g_terminal_type_buf above.
static char g_cc_source_buf[FS_MAX_FILE_SIZE + 1];
static unsigned char g_cc_save_buf[FS_MAX_FILE_SIZE];
static unsigned char g_cc_run_buf[FS_MAX_FILE_SIZE];

static int cmd_cc(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	if (argc < 3) {
		tp(s, "uso: cc <arquivo.c> <saida>");
		return 0;
	}
	int id = fs_resolve_path(s->cwd, argv[1]);
	if (id < 0 || id == FS_ROOT || !fs_get(id) || fs_get(id)->type != FS_TYPE_FILE) {
		tp(s, "arquivo nao encontrado");
		return 0;
	}
	int n = fs_read_file(id, g_cc_source_buf, FS_MAX_FILE_SIZE);
	if (n < 0) n = 0;
	g_cc_source_buf[n] = 0;

	unsigned char *code;
	char err_msg[64];
	int err_line;
	int code_len = cc_compile(g_cc_source_buf, n, cc_builtin_print, &code, err_msg, sizeof(err_msg), &err_line);
	if (code_len < 0) {
		textbox_puts(&s->tb, "erro (linha ");
		char lb[8];
		itoa(lb, err_line);
		textbox_puts(&s->tb, lb);
		textbox_puts(&s->tb, "): ");
		textbox_puts(&s->tb, err_msg);
		textbox_putc(&s->tb, '\n');
		return 0;
	}

	int total = cc_save_program(g_cc_save_buf, sizeof(g_cc_save_buf), code, code_len);
	if (total < 0) {
		tp(s, "erro: programa compilado nao cabe em um arquivo (max 4088 bytes de codigo)");
		return 0;
	}
	int out_id = fs_create_file(s->cwd, argv[2]);
	if (out_id < 0 || fs_write_file(out_id, (const char *)g_cc_save_buf, total) < 0) {
		tp(s, "nao foi possivel salvar o arquivo (espaco insuficiente ou nome ja existe)");
		return 0;
	}

	textbox_puts(&s->tb, "compilado: ");
	char nb[8];
	itoa(nb, code_len);
	textbox_puts(&s->tb, nb);
	textbox_puts(&s->tb, " bytes -> ");
	textbox_puts(&s->tb, argv[2]);
	textbox_putc(&s->tb, '\n');
	return 0;
}

static int cmd_run(wm_window_st *win, terminal_state_t *s, int argc, char **argv) {
	(void)win;
	if (argc < 2) {
		tp(s, "uso: run <arquivo>");
		return 0;
	}
	int id = fs_resolve_path(s->cwd, argv[1]);
	if (id < 0 || id == FS_ROOT || !fs_get(id) || fs_get(id)->type != FS_TYPE_FILE) {
		tp(s, "arquivo nao encontrado");
		return 0;
	}
	int n = fs_read_file(id, (char *)g_cc_run_buf, FS_MAX_FILE_SIZE);
	if (n < 0) n = 0;

	unsigned char *code;
	int code_len = cc_load_program(g_cc_run_buf, n, &code);
	if (code_len < 0) {
		tp(s, "erro: arquivo nao e um programa compilado valido");
		return 0;
	}

	g_cc_print_target = s;
	int (*fn)(void) = (int (*)(void))code;
	int result = fn();
	g_cc_print_target = 0;

	textbox_puts(&s->tb, "programa retornou: ");
	char rb[12];
	itoa(rb, result);
	textbox_puts(&s->tb, rb);
	textbox_putc(&s->tb, '\n');
	return 0;
}

static const terminal_cmd_t g_commands[] = {
	{"dir", cmd_dir, "lista arquivos e pastas do diretorio atual"},
	{"cd", cmd_cd, "muda ou mostra o diretorio atual"},
	{"chdir", cmd_cd, "muda ou mostra o diretorio atual"},
	{"md", cmd_md, "cria uma pasta"},
	{"mkdir", cmd_md, "cria uma pasta"},
	{"rd", cmd_rd, "remove uma pasta vazia"},
	{"rmdir", cmd_rd, "remove uma pasta vazia"},
	{"del", cmd_del, "apaga um arquivo"},
	{"erase", cmd_del, "apaga um arquivo"},
	{"ren", cmd_ren, "renomeia um arquivo ou pasta"},
	{"rename", cmd_ren, "renomeia um arquivo ou pasta"},
	{"copy", cmd_copy, "copia um arquivo"},
	{"type", cmd_type, "mostra o conteudo de um arquivo"},
	{"cc", cmd_cc, "compila um arquivo .c (subconjunto) e salva o programa compilado"},
	{"run", cmd_run, "executa um programa compilado salvo com cc"},
	{"cls", cmd_clear, "limpa a tela"},
	{"clear", cmd_clear, "limpa a tela"},
	{"date", cmd_date, "mostra a data atual"},
	{"time", cmd_time, "mostra a hora atual"},
	{"ver", cmd_ver, "mostra a versao do sistema"},
	{"vol", cmd_vol, "mostra o espaco livre em disco"},
	{"echo", cmd_echo, "repete o texto informado"},
	{"info", cmd_info, "informacoes do terminal"},
	{"help", cmd_help, "lista os comandos"},
	{"exit", cmd_exit, "fecha o terminal"},
	{0, 0, 0}
};

// Returns 1 if the window was closed (wm_close_window already kfree'd state
// via terminal_on_close) -- the caller must not touch state again.
static int terminal_exec(wm_window_st *win, terminal_state_t *s, char *line) {
	char *argv[TERMINAL_MAX_ARGS];
	int argc = terminal_tokenize(line, argv, TERMINAL_MAX_ARGS);
	if (argc == 0) {
		return 0;
	}
	for (int i = 0; g_commands[i].name; i++) {
		if (strcmp(argv[0], (char *)g_commands[i].name) == 0) {
			return g_commands[i].fn(win, s, argc, argv);
		}
	}
	textbox_puts(&s->tb, "comando desconhecido\n");
	return 0;
}

static void *terminal_on_init(wm_window_st *win) {
	(void)win;
	terminal_state_t *s = (terminal_state_t *)kmalloc(sizeof(terminal_state_t));
	textbox_init(&s->tb, TERMINAL_COLS, TERMINAL_ROWS);
	s->blink_on = 1;
	s->last_blink_tick = 0;
	s->cwd = FS_ROOT;
	s->history_count = 0;
	s->history_head = 0;
	s->history_pos = -1;
	textbox_puts(&s->tb, "rSystemOS terminal -- 'help' para comandos\n");
	terminal_print_prompt(s);
	return s;
}

static void terminal_on_show(wm_window_st *win, void *state, rect_st content) {
	(void)win;
	terminal_state_t *s = (terminal_state_t *)state;
	fill_rect(content.x, content.y, content.w, content.h, GUI_COLOR_BLACK);
	textbox_draw(&s->tb, content.x + TERMINAL_PAD, content.y + TERMINAL_PAD, s->blink_on);
}

static void terminal_on_key_down(wm_window_st *win, void *state, int ascii, int mods) {
	(void)mods;
	terminal_state_t *s = (terminal_state_t *)state;

	if (ascii == '\n') {
		// Move the textbox's real cursor to the end of the displayed input
		// before newlining, regardless of where the edit cursor sits.
		int end = s->prompt_col + s->input_len;
		s->tb.cursor_row = s->prompt_row + end / s->tb.cols;
		s->tb.cursor_col = end % s->tb.cols;
		if (s->tb.cursor_row >= s->tb.rows) {
			s->tb.cursor_row = s->tb.rows - 1;
		}
		textbox_putc(&s->tb, '\n');

		s->input_line[s->input_len] = 0;
		terminal_history_push(s);
		int closed = terminal_exec(win, s, s->input_line);
		if (closed) {
			return;
		}
		terminal_print_prompt(s);
		return;
	}

	if (ascii == KEY_UP) {
		if (s->history_pos + 1 < s->history_count) {
			s->history_pos++;
			terminal_history_load(s, s->history_pos);
			terminal_redraw_input(s);
		}
		return;
	}
	if (ascii == KEY_DOWN) {
		if (s->history_pos > 0) {
			s->history_pos--;
			terminal_history_load(s, s->history_pos);
			terminal_redraw_input(s);
		} else if (s->history_pos == 0) {
			s->history_pos = -1;
			s->input_len = 0;
			s->cursor = 0;
			terminal_redraw_input(s);
		}
		return;
	}

	if (terminal_input_key(s, ascii)) {
		terminal_redraw_input(s);
	}
}

static int terminal_on_tick(wm_window_st *win, void *state) {
	(void)win;
	terminal_state_t *s = (terminal_state_t *)state;
	unsigned int now = timer_get_ticks();
	if (now - s->last_blink_tick >= TERMINAL_BLINK_TICKS) {
		s->blink_on = !s->blink_on;
		s->last_blink_tick = now;
		return 1;
	}
	return 0;
}

static void terminal_on_close(wm_window_st *win, void *state) {
	(void)win;
	terminal_state_t *s = (terminal_state_t *)state;
	textbox_free(&s->tb);
	kfree(s);
}

const app_st terminal_app = {
	.name = "Terminal",
	.icon = ICON_TERMINAL,
	.on_init = terminal_on_init,
	.on_show = terminal_on_show,
	.on_key_down = terminal_on_key_down,
	.on_mouse_down = 0,
	.on_mouse_up = 0,
	.on_mouse_move = 0,
	.on_tick = terminal_on_tick,
	.on_close = terminal_on_close,
};
