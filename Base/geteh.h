/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#ifndef _H_geteh
#define _H_geteh
#include "kktypes.h"

extern int getpage(const uchar *cda);  /* Returns get_return */
extern int getnode(uchar *cda);  /* Returns get_return */
 
struct codepcfa {
   int code;
   PCFA *pcfa;
};
 
  /* The following get_returns may be returned in a struct codepcfa */
  /* or as a simple int as from getpage                             */
#define get_wait 0        /* Actor placed on wait queue */
#define get_ioerror 1     /* I/O error reading page or node */
#define get_tryagain 2    /* Look for page or node again */
#define get_virtualzero 3 /* Page is virtual zero */
#define get_notmounted 4  /* CDA not mounted (some return get_wait) */
#define get_gotpcfa 5     /* pcfa for the page/node has been returned */
 
extern struct codepcfa getalid(uchar *cda);
extern struct codepcfa getbvp(uchar *cda);
 
struct CodeDiskNode {
   int code;
   struct DiskNode *disknode;
};
 
  /* The following get_returns may, in addition to the ones above,  */
  /* be returned in a struct CodeDiskNode.                          */
#define get_gotdisknode 6 /* Pointer to the disk node is returned */
 
extern struct CodeDiskNode getbvn(uchar *cda);
 
extern CTE * gcleanmf(CTE *cte);   /* Try to copy a KRO page */
 
extern void gcktkckp(unsigned int reason);
#include "ckptcdsh.h"      /* Define code for checkpoint from key call */

extern int gdiqds(void);  /* Query if directory space low */

extern void grtcdap0(void);  /* Initialize read of mounted ranges */

extern int grtcdap1(void);   /* Extract data from next mounted ranges */

extern int grtisipl(CDA cda); /* Is CDA a part of a mounted IPL range */
#endif
