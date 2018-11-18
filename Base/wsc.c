/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "sysdefs.h"
#include "keyh.h"
#include "wsh.h"
 
 
unsigned long maxdib;
struct DIB *firstdib;
struct DIB *free_dib_head;
 
void (*waitstateprocess)();  /* function to call when no domains
                                are running */
 
struct Key dk0 = {{{0, {0,0,0,0,0,0,0,0,0,0,0}}}, 0, datakey};
struct Key dk1 = {{{0, {0,0,0,0,0,0,0,0,0,0,1}}}, 0, datakey};
 
NODE **anodechainheads;
unsigned long nodechainhashmask;
NODE *anodeend; /* ptr to end of node space */
NODE *firstnode;
 
CTE **apagechainheads;
unsigned long pagechainhashmask;
CTE *apageend;
CTE *firstcte, *lastcte;
 
unsigned long first_user_page; /* The first page byond the kernel */
unsigned long endmemory;   /* The last bus address + 1 */

int migrationpriority;

unsigned char currentswaparea; /* == SWAPAREA1 - Swap area 1 is current */
                               /* != SWAPAREA1 - Swap area 2 is current */

