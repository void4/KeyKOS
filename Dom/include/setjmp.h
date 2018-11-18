/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_setjmp
#define _H_setjmp

extern int setjmp();
extern void longjmp();

typedef long long jmp_buf[11];

#endif
