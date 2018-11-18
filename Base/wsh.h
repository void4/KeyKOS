/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef WSH_H
#define WSH_H
/* These variables are constant after initialization */
 
extern struct Key dk0, dk1;
 
extern NODE **anodechainheads;  /* a pointer to (the beginning of
                the array of) pointers to NODEs */
extern unsigned long nodechainhashmask;
extern NODE *anodeend;
extern NODE *firstnode;
 
extern CTE **apagechainheads;  /* a pointer to (the beginning of
                the array of) pointers to CTEs */
extern unsigned long pagechainhashmask;
extern CTE *apageend;
extern CTE *firstcte, *lastcte;
/* The CTEs between apageend and lastcte describe the pixel buffers. */
 
extern unsigned long first_user_page; /* First page after the kernel */
extern unsigned long endmemory;       /* The highest bus address + 1 */
 
 
extern unsigned long maxdib;
extern struct DIB *firstdib;
extern struct DIB *free_dib_head;
 
extern void (*waitstateprocess)();
extern int migrationpriority;

#define SWAPAREA1 255      /* Must match RANGETABLESWAPAREA1 */
extern
unsigned char currentswaparea; /* == SWAPAREA1 - Swap area 1 is current */
                               /* != SWAPAREA1 - Swap area 2 is current */

#endif /* WSH_H */
