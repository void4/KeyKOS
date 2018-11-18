/* Definitions of routines in Machine Independent disk I/O called by */
/* the machine dependent system */
 
#include "pdrh.h"
#include "devmdh.h"
#include "kktypes.h"
extern void gdinperr(CTE *cte);
extern void gdiset(CTE *cte, RANGELOC swaploc);

void getqnodp(CTE *cte);

extern int grtadd(PDRD *pdrd, DEVICE *dev);
   /* Returns 0 iff could not be added. */
extern void grtclear(DEVICE *dev);
/* See also gbadlog in gbadpagh.h,
            prioritytable in gmigrath.h,
            checkforcleanstart in getih.h */
