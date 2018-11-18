/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>

int sparc_lineb_putchar(char);
char sparc_lineb_getchar();

#if 0
static void put_charstring(char *ptr)
{
        while(*ptr) {
            sparc_lineb_putchar(*ptr);
            if(*ptr == '\n') sparc_lineb_putchar('\r');
            ptr++;
        }
}

static void get_echo_charstring(char *ptr)
{
        char c;

        while(1) {
            c=sparc_lineb_getchar();
            sparc_lineb_putchar(c);
            if(c == '\r') {
                  sparc_lineb_putchar('\r');
                  break;
            }
            *ptr = c;
            ptr++;
        }
        *ptr=0;         
}

void sparc_gdb_enter()   // from console interupt to plant breakpoints
{
        char buf[256];

        while(1) {
	   put_charstring("\nGDB:");
           get_echo_charstring(buf);
           if(!Strcmp(buf,"go")) break;
        }
        omak_default_breakpt();
        return;
}
#endif


