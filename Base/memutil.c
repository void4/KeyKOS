/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "types.h"
#include "memutil.h"

#ifndef NULL
#define NULL 0
#endif

void *
Memset(void *sp1, int c, int n)
{
	uchar_t *sp = sp1;

	if (n == 0)
		return sp1;
	
	while (n > 0) {
		*sp++ = (uchar_t) c;
		n--;
	}
	return sp1;
}

int
Memcmp(const void *s1, const void *s2, int n)
{
	const uchar_t *ps1 = s1;
	const uchar_t *ps2 = s2;

	if (s1 == s2 || n == 0)
		return 0;

	while (n > 0) {
		if (*ps1++ != *ps2++)
			return(*(ps1-1) - *(ps2-1));
		n--;
	}
        return 0;
}

typedef unsigned int u32;
void * Memcpy(void * to, const void * from, int n){
u32 f = (u32)from, t = (u32)to;
while((t & 3) && n) {*(char*)t++ = *(char*)f++; --n;}
// Now either n == 0 or t is on 4 byte boundary.
if(!n) return to;
// t is on word boundary!
if(f&3){char lb = f&3;
   char sa = lb << 3;
   f &= ~3; // Round to word boundary.
   {u32 d = *(u32*)f << sa;
   while(n & ~3){u32 e = *++(u32*)f;
     *((u32*)t)++ = d | e >> (32 - sa);
      n-=4; d = e<< sa;}
   f |= lb;}}
else while(n & ~3) {*((u32*)t)++ = *((u32*)f)++; n-=4;}
while(n--) *(char*)t++ = *(char*)f++;
return to;}
