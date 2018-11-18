/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "ioreqsh.h"
#include "devmdh.h"
#include "pdrh.h"
/********************************************************************
md_dskdevreqaddr - Compute machine dependent address from address
 
Input -
   devreq - Pointer to devreq with address and device fields set up
 
Output -
   sets devreq->md_address. May be a nop in some implementations
********************************************************************/
#define md_dskdevreqaddr(devreq)
 
 
/********************************************************************
offset2cyl - Compute "cylinder" number from page offset
 
Input -
   offset - Page offset on device
 
Output -
   Cylinder number
extern uint32 offset2cyl(uint32 offset);
********************************************************************/
#define offset2cyl(offset) (offset / ((77*12)/8)) /* for DK312C-25 */
 
bool gdddovv(    /* Do volume verification, mount disk */
   PHYSDEV *pdev,
   unsigned long physoffset,
   unsigned long nblks,
   PDR *pdr);
extern void gddenq(REQUEST *req);  /* Enqueue a request */
 
extern int gddstartpageclean(void);    /* Zero if nothing cleaned */
 
extern void gddabtdr(DEVREQ *drq); /* Abort a devreq */
 
extern void gccicln(void);         /* New group of frames to clean */
 
extern int gccbncs(struct Device *dev, CTE *cte); /* Add to group */
                                   /* 0 if could not add to group */
 
extern void gcodismt(struct Device *dev);  /* Dismount device */
void free_any_page(REQUEST *req); /* Free any page allocated
                    by low-level io system */

