#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/video.h"
#include "../include/stdio.h"
#include "../include/keyboard.h"
#include <stdio.h>

void startcalc();
static void terminal_shell();
static void gui_wait_key();
static void gui_draw_menu(int selected);
static void gui_exec_selected(int selected);
static void gui_show_info();
static void gui_draw_button(int x, int y, int w, int h, int selected, char* text);

void main() {
	int selected = 0;
	gui_draw_menu(selected);

	while (1) {
		char key = 0;
		while ((key = read_keyborad()) == 0) { /* wait */ }

		// Navigate with W/S (Enter to select)
		if (key == 'w' || key == 'W') {
			selected = (selected + 3) % 4;
			gui_draw_menu(selected);
			continue;
		}
		if (key == 's' || key == 'S') {
			selected = (selected + 1) % 4;
			gui_draw_menu(selected);
			continue;
		}
		if (key == '\n') {
			gui_exec_selected(selected);
			gui_draw_menu(selected);
			continue;
		}
	}
}

static void terminal_shell() {
	clear_screen();
	kernel_print_text("=== KERNEL TERMINAL ===\n", 0);
	kernel_print_text("Commands: help, clear, echo, info, exit\n\n", 0);

	int running = 1;
	while (running) {
		kernel_print_text("# ", 0);
		
		char cmd[64];
		scan(cmd, 64);
		
		kernel_print_text("\n", 0);

		// help command
		if (strcmp(cmd, "help") == 0) {
			kernel_print_text("Available commands:\n", 0);
			kernel_print_text("  help  - Show this help\n", 0);
			kernel_print_text("  clear - Clear screen\n", 0);
			kernel_print_text("  echo  - Echo text (usage: echo <text>)\n", 0);
			kernel_print_text("  info  - Show system info\n", 0);
			kernel_print_text("  exit  - Exit terminal\n\n", 0);
		}
		// clear command
		else if (strcmp(cmd, "clear") == 0) {
			clear_screen();
		}
		// echo command (basic implementation)
		else if (cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o') {
			if (cmd[4] == ' ') {
				int i = 5;
				while (cmd[i] != '\0') {
					kernel_print_char(cmd[i], 0);
					i++;
				}
				kernel_print_text("\n\n", 0);
			} else {
				kernel_print_text("echo: syntax error\n\n", 0);
			}
		}
		// info command
		else if (strcmp(cmd, "info") == 0) {
			kernel_print_text("=== System Info ===\n", 0);
			kernel_print_text("OS: rSystemOS v0.2\n", 0);
			kernel_print_text("Mode: 32-bit Protected Mode\n", 0);
			kernel_print_text("Video: Mode 13h (320x200x8)\n\n", 0);
		}
		// exit command
		else if (strcmp(cmd, "exit") == 0) {
			running = 0;
		}
		// unknown command
		else if (strlen(cmd) > 0) {
			kernel_print_text("Command not found: ", 0);
			kernel_print_text(cmd, 0);
			kernel_print_text("\n\n", 0);
		}
	}
}

void startcalc()
 {	
	clear_screen();
	next_line();
	kernel_print_text("Informe o primeiro valor: ",0);

	int stat = 1;
	while (stat) {
		char ac[10];
		scan(ac,10);
		int a = atoi(ac);

		kernel_print_text("\nInforme o segundo valor: ",0);
		char bc[10];
		scan(bc,10);
		int b = atoi(bc);

		kernel_print_text("\nInforme o operador (+, -, *, /): ",0);
		char op[2];
		scan(op,2);

		char res[10];
		switch(op[0]) {
			case '-':
				itoa(res, (a - b));
				kernel_print_text("\n",0);
				kernel_print_text(res,0);
				break;

			case '+':
				itoa(res, (a + b));
				kernel_print_text("\n",0);
				kernel_print_text(res,0);
				break;

			case '*':
				itoa(res, (a * b));
				kernel_print_text("\n",0);
				kernel_print_text(res,0);
				break;

			case '/':
				itoa(res, (a / b));
				kernel_print_text("\n",0);
				kernel_print_text(res,0);
				break;

			case 'q':
				stat=0;
				break;
		}

		if (stat) kernel_print_text("\nInforme o primeiro valor: ",0);
	}
 }

static void gui_draw_button(int x, int y, int w, int h, int selected, char* text) {
	// Colors are palette indices (works even if the palette is default).
	unsigned char bg = selected ? 10 : 4;
	unsigned char border = selected ? 15 : 8;
	unsigned char fg = 15;

	fill_rect(x, y, w, h, bg);
	draw_rect(x, y, w, h, border);

	// Center text in button
	int text_len = strlen(text);
	int text_width = text_len * FONT_W;
	int tx = x + (w / 2) - (text_width / 2);
	int ty = y + (h / 2) - (FONT_H / 2);

	draw_text(tx, ty, text, fg);
}

static void gui_draw_menu(int selected) {
	clear_screen();

	// Four big buttons with text labels
	int bx = 60;
	int bw = 200;
	int bh = 42;
	int gap = 12;
	int by = 40;

	gui_draw_button(bx, by + 0 * (bh + gap), bw, bh, selected == 0, "1 - Calculator");
	gui_draw_button(bx, by + 1 * (bh + gap), bw, bh, selected == 1, "2 - Info");
	gui_draw_button(bx, by + 2 * (bh + gap), bw, bh, selected == 2, "3 - Terminal");
	gui_draw_button(bx, by + 3 * (bh + gap), bw, bh, selected == 3, "4 - Shutdown");
}

static void gui_wait_key() {
	char key = 0;
	while ((key = read_keyborad()) == 0) { /* wait */ }
}

static void gui_show_info() {
	clear_screen();

	// Numeric-only info to avoid depending on missing glyphs.
	draw_text(150, 96, "0.2", 15);
	draw_text(150, 112, "2", 15);

	gui_wait_key();
}

static void gui_exec_selected(int selected) {
	if (selected == 0) {
		startcalc();
		return;
	}
	if (selected == 1) {
		gui_show_info();
		return;
	}
	if (selected == 2) {
		terminal_shell();
		return;
	}

	shutdown();
}