#include "../include/string.h"

int atoi(char *string)
{
	char a=0;
	int res=0;
	while((a= *string++)!='\0')
	{
		res = res*10 + a - '0';
	}
	return res;
}

void itoa(int n, char s[])
{
	int i, sign;

	if ((sign = n) < 0)
		n = -n;

	i = 0;

	do {
		s[i++] = n % 10 + '0';
	} while ((n /= 10) > 0);

	if (sign < 0)
		s[i++] = '-';

	s[i] = '\0';

	reverse(s);
}

void sleep(int milisecond)
{
	extern void kernel_sleep(int);
	kernel_sleep(milisecond);
}

void shutdown()
{
	extern void kernel_shutdown();
	kernel_shutdown();
}