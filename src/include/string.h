#ifndef _STRING_H
#define _STRING_H

int strlen(char *str);
void reverse(char *s);
int strcmp(char *str1, char *str2);
void strcpy(char* buf_to, char* buf_from);

// Added alongside the terminal CLI rewrite -- strcmp() above only ever
// returns 0/1 (equal or not) and is left untouched since it's used for
// equality checks everywhere; this is a separate, standard-shaped helper
// (ordered -1/0/1 compare) used for sorting.
int strncmp(const char *str1, const char *str2, int n);
void *memcpy(void *dst, const void *src, int n);
void *memset(void *dst, int c, int n);

#endif  