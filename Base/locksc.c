/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include "keyh.h"
#include "locksh.h"
 
int preplock_node(
   NODE *node,     /* The node to lock */
   char lockerid)         /* The id of the locking routine */
      /* See "locksh" for assigned lockerids */
/*
   Output -
      0 - Lock was available and is now held
      1 - Node was already locked
*/
{
   if (node->preplock & 0x80) return 1; /* Already locked */
   node->preplock = lockerid;
   return 0;
}

void coreunlock_page(int k, CTE *cte)
{
 if(cte->corelock < 8) /* it would underflow */
      crash("Unlocking unlocked page");
 cte->corelock = (cte->corelock-8)|HILRU; 
 if (!(cte->corelock & 0xf8))
   cte->unlockid = k; /* who unlocked to zero */
}
