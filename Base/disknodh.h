/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_disknodh
#define _H_disknodh
#include "diskkeyh.h"
#include "keyh.h" /* for pagesize */

/* The following are used in the flags field of the DiskNode Structure*/
#define DNINTEGRITY    0xe0      /* See nodepot.checkbyte below */
#define DNINTEGRITYONE 0x20      /* Increment for integrity field */
#define DNPROCESS      0x08      /* Node has a process */
#define DNGRATIS       0x01      /* Node is treated as gratis */

typedef struct DiskNode {
	CDA cda;			/* CDA of the node */
	unsigned char flags;
	unsigned char allocationid[4];	/* Allocation ID of the node */
	unsigned char callid[4];	/* Call ID of the node */
	DISKKEY keys[16];		/* The keys */
} DiskNode_t;
 
#define NPNODECOUNT ((pagesize-1)/(sizeof(struct DiskNode)+1))
 
typedef struct NodePot {
	struct DiskNode disknodes[NPNODECOUNT];
	unsigned char migratedthismigration[NPNODECOUNT];
	unsigned char checkbyte;
	/*  Check byte for correct write.  The first 3 bits must */
	/*  match the first 3 bits of the first flag field. The  */
	/*  last 5 bits are the inverse of the previous byte to  */
	/*  check for controlers that replicate bytes on overrun. */
} NodePot_t;
#endif
