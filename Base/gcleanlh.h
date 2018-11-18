#include "devmdh.h"
extern int gcladd(CTE *cte);   /* !0 if page cleaning should start */
extern int gclctpn(CTE *cte);  /* !0 if page cleaned, 0 if still dirty */
extern void gclfreze(int p);   /* Freeze (=1) or unfreeze (=0) list */
int gclbuild(DEVICE *dev);     /* !0 if anything built */
