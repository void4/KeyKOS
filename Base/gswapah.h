#include "grangeth.h"

extern struct Device *gswfbest(int maxstate);
 
extern struct Device *gswnbest(int maxstate);
 
extern void gswckmp(void);   /* Check migration priority */

extern struct CodeGRTRet gswnext(struct Device *);
 /* Returns grt_mustread if space available or */
 /*      grt_notmounted if there is no space available */
