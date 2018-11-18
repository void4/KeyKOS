/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */
 
 
#include "sysdefs.h"
#include "kktypes.h"
#include "lli.h"
#include "keyh.h"
#include "wsh.h"
#include "cpujumph.h"
#include "gateh.h"
#include "primcomh.h"
#include "prepkeyh.h"
#include "queuesh.h"
#include "memomdh.h"
#include "geteh.h"
#include "kernelpk.h"
#include "timemdh.h"
#include "kernkeyh.h"
 
 
 
/* Local static constants */
 
static const CDA cdaone  = {0, 0, 0, 0, 0, 1};
 
 
 
/**********************************************************************
jckfckpt - Checkpoint key
 
  Input -
     key    - Holds a pointer to the invoked key.
     in addition:
        cpudibp - has the jumper's DIB
        cpuordercode - has order code.
        The invoked key type is a checkpoint miscallaneous key
 
  Output - None
**********************************************************************/
void jckfckpt(void)    /* Handle jumps to checkpoint key */
{
   uint64 wanted, now;           /* times controling the checkpoint */
   struct KernelPage *kp;     /* To kernel page */
   CTE *cte;                  /* For kernel page */
 
   if (cpuordercode > 1) {    /* N.B. KT value is also KT+2 */
      simplest(KT+2);
      return;
   }
   cte = srchpage(cdaone);    /* Look for the kernel page */
   if (!cte) {                /* Not in storage */
      switch (getpage(cdaone)) {
       case get_wait:       /* Actor has been queued */
         abandonj();
         return;
       case get_ioerror:    /* I/O error reading page */
         crash("JCKPTC001 Permanent I/O error on kernel page");
       case get_tryagain:   /* Look again for the page*/
         cte = srchpage(cdaone);  /* Find it now */
       break;
       default:
         crash("JCKPTC002 Unexpected return code from getpage ");
      }
   }
   kp = (struct KernelPage *)
                  map_window(CKPMIGWINDOW, cte, MAP_WINDOW_RO);
   pad_move_arg((void*)&wanted, 8);
   if (wanted <= kp->KP_LastCheckPointTOD) {
      /* Last checkpoint is recent enough for caller */
      simplest(0);
      return;
   }
   now = read_system_timer();
   if (now < wanted) {  /* His time is in the future */
      simplest(1);
      return;
   }
   /* Caller waits until new checkpoint is established */
   enqueuedom(cpuactor, &migratewaitqueue);
   if (cpuordercode) gcktkckp(TKCKPKEYCALL); /* Force ckpt iff asked */
   abandonj();
   return;
} /* end jcktkckp */
