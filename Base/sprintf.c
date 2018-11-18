/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* prom_printf */

#include <stdarg.h>

static char numBuffer[64];


static void vxprintf(char *buf, char *format, va_list va)
{
  int ch;
  char *s, *p = buf;
  const char *h = 0;

  int width,nblen,leadzero;

  int i;
  unsigned u;

  while ((ch = *format++) != 0) {
    width=1;
    leadzero=0;
    switch (ch) {
    case '%':
      if((*format == '-')) { // justification
          format++;
      }
      if((*format >= '0') && (*format <= '9')) { // number  could be leading 0
          width = *format - '0';
          format++;
      }
      if((*format >= '0') && (*format <= '9')) { // number
          if(!width) leadzero=1;    // width was preceeded by 0
          width = *format - '0'; 
          format++;
      }
      if((*format == 'l') || (*format == 'L')) {   // "l" modifier
          format++;
      }
      switch (ch = *format++) {
      case 's':
        nblen=0;
	s = va_arg(va, char*);
	while ((ch = *s++) != 0) {
	  *p++ = ch;
          nblen++;
	}
        while(width > nblen) {
            *p++ = ' ';
            nblen++;
        }
            
	break;
      case 'd':
      case 'u':
	i = va_arg(va, int);
	if (ch == 'd' && i < 0) {
	  i = -i;
	  *p++ = '-';
	}
	s = numBuffer;
	do {
	  *s++ = '0' + (char) (i % 10);
	} while ((i /= 10) != 0);
	while (s > numBuffer) {
	  *p++ = *--s;
	}
	break;
      case 'x':
      case 'X':
	h = "0123456789abcdef";
	if (ch == 'X') {
	  h = "0123456789ABCDEF";
	}
	u = va_arg(va, unsigned int);
	s = numBuffer;
	do {
	  *s++ = h[u & 0xF];
	} while ((u >>= 4) != 0);

        nblen=s-numBuffer;
        i=width-nblen;
        if(i>0) {
           while(i) {
             if(leadzero) *p++ = '0';
             else *p++ = ' ';
             i--;
           }    
        }

	while (s > numBuffer) {
	  *p++ = *--s;
	}
	break;
      case 'o':
	u = va_arg(va, unsigned int);
	s = numBuffer;
	do {
	  *s++ = '0' + (char) (u & 7);
	} while ((u >>= 3) != 0);
	while (s > numBuffer) {
	  *p++ = *--s;
	}
	break;
      case 'f':
      case 'g':
      default:
		  ;
      }
      break;

    case '\n':
      *p++ = '\r';
      /* NO BREAK because \n translates to cr-lf */

    default:
      *p++ = (char) ch;
    }
  }
  *p=0;

}

void Sprintf(char * buf, char *format, ...)
{
  va_list va;
  va_start(va, format);

  vxprintf(buf,format, va);

  va_end(va);
}
