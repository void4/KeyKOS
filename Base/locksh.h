/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef LOCKSH_H
#define LOCKSH_H

#include "sysdefs.h"
#include "keyh.h"

extern int preplock_node(
   NODE *n, char lockerid
   );
/*
   Output -
      0 - Lock was available and is now held
      1 - Node was already locked
*/
#define lockedby_selectdomain 0xf0
#define lockedby_unprnode 0xf8
#define lockedby_getreturnee 0xe0
#define lockedby_ensuredestpage 0xe8
#define lockedby_prepreturnee 0xd0
#define lockedby_jdomain 0xd8
#define lockedby_jresume 0xc0
#define lockedby_ensureprepnode 0xc8
#define lockedby_prepmeter 0xb0
#define lockedby_prepdom 0xb8
#define lockedby_space 0xa0
#define lockedby_asm_jresume 0xa8
#define lockedby_asm_dom_trap 0x90
#define lockedby_asm_jdomain 0x98
 
#define HILRU 3     /* Must be power of 2 minus 1 */
 
/* extern void unpreplock_node(NODE *n); */
#define unpreplock_node(n) ((n)->preplock = HILRU)
 
/* extern void corelock_node(const int id, const NODE *n); */
/*#define corelock_node(id, n) ((n)->corelock += 1)*/
#define corelock_node(id, n)                                      \
  {if((n)->corelock > 200) crash("Excessive node lock");            \
   (n)->fill1 = (((n)->fill1+1) & 0xffff)                         \
               | (((n)->fill1>>8) & 0xff0000) | (id<<24);             \
   ++(n)->corelock;}
 
/* extern void coreunlock_node(NODE *n); */
/* #define coreunlock_node(n) ((n)->corelock-=1, (n)->preplock|=HILRU)*/
#define coreunlock_node(n)                                        \
    {if((signed char)n->corelock<=0) crash("Bad core_unlock");    \
    n->fill1 = ((n->fill1-1) & 0xffff) | (n->fill1 & 0xffff0000); \
    n->corelock--; n->preplock|=HILRU;}
 
/* extern void corelock_page(CTE *cte); */
#define corelock_page(cte) ((cte)->corelock += 8)
/*static void corelock_page(CTE *cte)
{
 if (cte->corelock > 0x78) crash("lock page overflow");
 cte->corelock += 8;
}*/
 
/*#define coreunlock_page(cte) ((cte)->corelock = (cte)->corelock-8|HILRU)*/
void coreunlock_page(int k, CTE *cte);

#endif /* LOCKSH_H */
