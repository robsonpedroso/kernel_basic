#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/video.h"
#include "../include/stdio.h"
#include "../include/keyboard.h"
#include <stdio.h>

char* show="\nKernel basico para estudo\n";

void startcalc();

void main() {
	clear_screen();
	
	while(1)
	{
		char buf[20];
		scan(buf,10);

		if(strcmp(buf,"show")==0)
		{
			kernel_print_text(show, "#");
		}
		else if(strcmp(buf, "shutdown") == 0)
		{
			shutdown();
		}

		else if(strcmp(buf,"calc")==0)
		{
		  startcalc();
		  kernel_print_text("quited from calc\n",0);
		}
		else if(strcmp(buf,"help")==0)
		{
			kernel_print_text("commands: \ncalc-->start calc\nshow-->print info system\nhelp-->print that\nclear-->clear screen\nshutdown",0);
		}
		else if(strcmp(buf,"clear")==0)
		{
			clear_screen();
		}
		else if (strlen(buf)>0)kernel_print_text("unknown command\n",0);
	}
}

void startcalc()
 {	
	next_line();
	kernel_print_text("operand a,b.operation +,-,/,* q-quit from calc write ops with numpad\n",0);
		  
 	int stat=1;
 	while(stat)
 	{
		 next_line();
		kernel_print_text("a:",0);

		char ac[10];
		scan(ac,10);

		int a=atoi(ac);

		kernel_print_text("b:",0);

		char bc[10];
		scan(bc,10);
		int b=atoi(bc);

		kernel_print_text("op:",0);
		char op[2];
		scan(op,2);

		switch(op[0])
		{	char res[10];
			case '-':
				itoa((a-b),res);

				kernel_print_text(res,0);
				break;

			case '+':
				itoa((a+b),res);

				kernel_print_text(res,0);
				break;

			case '*':
				itoa((a*b),res);

				kernel_print_text(res,0);
				break;

			case '/':
				itoa((a/b),res);

				kernel_print_text(res,0);
				break;

			case 'q':
				stat=0;
				break;
		}
	}
 }