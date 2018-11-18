/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "keykos.h"
#include <stdio.h>

   KEY k0 = 15;
   KEY dom = 3;

   KEY t1;
   KEY t2;

   char data[256];

main()
{

	JUMPBUF;
	unsigned long oc,rc;
	short db;
	int actlen;

	t1=10;
	t2=11;
	KC (dom,64) KEYSTO(k0, t1) STRUCTTO(data) KEYSFROM(k0, t2)
		STRUCTFROM(data) RCTO(rc) DBTO(db);
}

jump_trap(a, b, c, d, e, f)
int a, b, c, d, e, f;
{
	printf("%%o0 = 0x%8.8x\n", a);
	printf("%%o1 = 0x%8.8x\n", b);
	printf("%%o2 = 0x%8.8x\n", c);
	printf("%%o3 = 0x%8.8x\n", d);
	printf("%%o4 = 0x%8.8x\n", e);
	printf("%%o5 = 0x%8.8x\n", f);
	exit(0);
}

