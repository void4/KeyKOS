/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  GUINTC - Set checking for "check read" pages - KeyTech Disk I/O */
 
#include <string.h>
#include <limits.h>
#include "lli.h"
#include "kktypes.h"
#include "sysdefs.h"
#include "keyh.h"
#include "memomdh.h"
#include "guinth.h"
 
 
 
/*********************************************************************
guintcr - Update read check information for a page
 
  Input -
     cte     - Pointer to the CTE for the page
 
  Output - None
*********************************************************************/
void guintcr(CTE *cte)
{
   if (cte->flags & ctcheckread) {
      char flags = cte->flags;      /* Save page flags */
      uint32 *p = (uint32 *)map_window(QUICKWINDOW, cte, MAP_WINDOW_RW);
 
      *p += 1;                               /* Increment first word */
      *(p + (pagesize/sizeof(uint32))-1) = *p; /* Copy to last word */
      cte->flags = flags;           /* Restore the flags */
   }
} /* End guintcr */
