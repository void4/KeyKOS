extern void gcktkckp(unsigned int reason);
#include "ckptcdsh.h"            /* Define valid reasons */

extern void gckincpc(CTE *cte);  /* Increment checkpoint page count */
                                 /* and make page Kernel R/O */

extern void gckdecpc(CTE *cte);  /* Decrement checkpoint page count */
                                 /* and reset page's Kernel R/O */
