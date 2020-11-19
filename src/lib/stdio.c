#include "../include/keyboard.h"
#include "../include/video.h"

void scan(char* buf, int n)
{
	char key= 0;
	int i= 0;
	char stat= 1;
	int startpos= get_cursor_pos()%80;

	while(stat) // '\n is enter press key code'
	{
		key = read_keyborad();
		if (key == 0) continue;
		switch(key)
		{
			case '\n'://if enter press
				kernel_print_char('\n',0);
				stat=0;
				break;

			case '\b':
				if(i>0)
				{
					int x=get_cursor_pos();
						rm_char_in_pos(x-1);

						i--; 
						buf--;
				}
				 
				//erase
				break;
			case '<':
				//move left cursor
				break;
			case '>':
				//move right
				break;

			default:
				if(i<n-1)
				{
					*buf++=key;
					kernel_print_char(key, 0);
					i++;
				}
				
				break;
		}
	}

	*(buf++)='\0';
}
