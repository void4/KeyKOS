/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "types.h"
#include "sysdefs.h"
#include <string.h>

void *
long2b(long i, uchar_t *str, int len) 
{  
	if (len > 4) {
		memset(str, 0, len-4);
		str += len-4;
		len = 4;
	}
	memcpy(str, (char *)&i+4-len, len);
	return str;
}

long 
b2long(const uchar_t *str, int len)
{  
	long v = 0L;
	if (len > 4) {
		str += len-4;
		len = 4;
	}
	memcpy((char *)&v+4-len, str, len);
	return v;
}
