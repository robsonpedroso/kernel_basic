int strlen(char *str)
{
	int len=0;
	while ((*str++)!='\0')len++;
	return len;
}

int strcmp(char *str1, char *str2)
{ 
	int len=strlen(str1);

	if(len!=strlen(str2))
	{
		return 1;
	}
	else
	{
		for(int i=0;i<len;i++)
			if(str1[i] != str2[i])
			{
				return 1;
			}
	}

	return 0;
}

void strcpy(char* buf_to, char* buf_from)
{
	char a=0;
	while((a= *buf_from++) != '\0')
	{
		*buf_to++ =a;
	}
}

void reverse(char s[])
 {
     int i, j;
     char c;

     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = c;
     }
 }

int strncmp(const char *str1, const char *str2, int n)
{
	int i = 0;
	while (i < n && str1[i] && str2[i]) {
		if (str1[i] != str2[i]) {
			return (unsigned char)str1[i] < (unsigned char)str2[i] ? -1 : 1;
		}
		i++;
	}
	if (i == n) {
		return 0;
	}
	if (str1[i] == str2[i]) {
		return 0;
	}
	return (unsigned char)str1[i] < (unsigned char)str2[i] ? -1 : 1;
}

void *memcpy(void *dst, const void *src, int n)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	for (int i = 0; i < n; i++) {
		d[i] = s[i];
	}
	return dst;
}

void *memset(void *dst, int c, int n)
{
	unsigned char *d = (unsigned char *)dst;
	for (int i = 0; i < n; i++) {
		d[i] = (unsigned char)c;
	}
	return dst;
}