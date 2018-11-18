/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* This file contains a watered down version of sprintf */

#include <sys/types.h>
#include <stdarg.h>

char *sprintf(char *buf, const char *fmt, ...);
char *vsprintf(char *buf, const char *fmt, va_list ap);
static char *printn(u_longlong_t n, int b, int width, int pad, 
	char *lbp, char *linebuf);

#define PRINTC(c) (*lbp++ = c, lbp)
/*PRINTFLIKE2*/
char *
sprintf(char *buf, const char *fmt, ...)
{
	va_list ap;
	char *rval;

	va_start(ap, fmt);
	rval = vsprintf(buf, fmt, ap);
	va_end(ap);
	return rval;
}
char *
vsprintf(char *buf, const char *fmt, va_list ap)
{
	char *lbp = buf;
	char *linebuf = buf;
	int pad, width, ells;
	int b, c, i, any;
	u_longlong_t ul;
	longlong_t l;
	char *s;

loop:
	while ((c = *fmt++) != '%') {
		lbp = PRINTC(c);
		if (c == '\0') {
			va_end(ap);
			return buf;
		}
	}

	c = *fmt++;
	for (pad = ' '; c == '0'; c = *fmt++) {
		pad = '0';
	}

	for (width = 0; c >= '0' && c <= '9'; c = *fmt++) {
		width = width * 10 + c - '0';
	}

	for (ells = 0; c == 'l'; c = *fmt++) {
		ells++;
	}

	switch (c) {
	case 'd': case 'D':
		b = 10;
		l = (ells <= 1) ? (longlong_t) va_arg(ap, long)
				: va_arg(ap, longlong_t);
		if (l < 0) {
			lbp = PRINTC('-');
			width--;
			ul = -l;
		} else {
			ul = l;
		}
		goto number;

	case 'x': case 'X':
		b = 16;
		goto u_number;

	case 'u':
		b = 10;
		goto u_number;

	case 'o': case 'O':
		b = 8;
u_number:
		ul = (ells <= 1) ? (u_longlong_t) va_arg(ap, u_long)
				: va_arg(ap, u_longlong_t);
number:
		lbp = printn((u_longlong_t) ul, b, width, pad,
		    lbp, linebuf);
		break;

	case 'c':
		b = va_arg(ap, int);
		for (i = 24; i >= 0; i -= 8)
			if ((c = ((b >> i) & 0x7f)) != 0) {
				if (c == '\n')
					lbp = PRINTC('\r');
				lbp = PRINTC(c);
			}
		break;

	case 'b':
		b = va_arg(ap, int);
		s = va_arg(ap, char *);
		lbp = printn((u_longlong_t) (unsigned) b, *s++, width, pad,
		    lbp, linebuf);
		any = 0;
		if (b) {
			while ((i = *s++) != 0) {
				if (b & (1 << (i-1))) {
					lbp = PRINTC(any? ',' : '<');
					any = 1;
					for (; (c = *s) > 32; s++)
						lbp = PRINTC(c);
				} else
					for (; *s > 32; s++)
						;
			}
			if (any)
				lbp = PRINTC('>');
		}
		break;

	case 's':
		s = va_arg(ap, char *);
		if (!s) {
			/* null string, be polite about it */
			s = "<null string>";
		}
		while ((c = *s++) != 0) {
			if (c == '\n')
				lbp = PRINTC('\r');
			lbp = PRINTC(c);
		}
		break;

	case '%':
		lbp = PRINTC('%');
		break;
	}
	goto loop;
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
static char *
printn(
	u_longlong_t n,
	int b,
	int width,
	int pad,
	char *lbp,
	char *linebuf)
{
	char prbuf[22];	/* sufficient for a 64 bit octal value */
	char *cp;

	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
		width--;
	} while (n);
	while (width-- > 0)
		*cp++ = pad;
	do {
		lbp = PRINTC(*--cp);
	} while (cp > prbuf);
	return (lbp);
}
