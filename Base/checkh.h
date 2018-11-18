/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "lli.h"

extern uint64 cvaddr;
extern NODE *chbgnode;
extern char memobject, entrytype;
extern unsigned char ckseapformat;
extern long entrylocator;
extern unsigned short entryhash;

Producer *check_memory_tree(     /* Check memory tree */
   Producer *item,        /* Node or page to check */
   int limit,                     /* Extent limit to check */
   unsigned int slot_origin);     /* slot/2 to start with */

Producer *check_memory_key(    /* Check a memory key */
   struct Key *key);           /* The key to check */
void check(void);
void checkrunning(void);
void check_seg_map(unsigned long m, char cid);

