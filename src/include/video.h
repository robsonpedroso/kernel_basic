#ifndef _VIDEO_H
#define _VIDEO_H

int get_cursor_pos();
void rm_char_in_pos(int);

void kernel_print_text(char*, char);
void kernel_print_char(char, char);

void scroll();

#endif  