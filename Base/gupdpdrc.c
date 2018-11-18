/*  GUPDPDRC - Update the PDR when status changes - KeyTech Disk I/O */
 
#include <string.h>
#include <limits.h>
#include "lli.h"
#include "sysdefs.h"
#include "keyh.h"
#include "queuesh.h"
#include "locksh.h"
#include "spaceh.h"
#include "memomdh.h"
#include "ioreqsh.h"
#include "ioworkh.h"
#include "dskiomdh.h"
#include "devmdh.h"
#include "getih.h"
#include "pdrh.h"
#include "grt2guph.h"
#include "gupdpdrh.h"
 
 
 
 
/* Local static variables */
 
static int transitcount = 0; /* Outstanding I/O operations */
static int pdrupdateactive;  /* Non-zero while domain level running */
 
 
 
/*********************************************************************
decrementtransitcount - Decrement counter and run waiters is zero
 
  Input - None
 
  Output - None
*********************************************************************/
static void decrementtransitcount(void)
{
   if (!transitcount)
      crash("GUPDPDRC001 transitcount underflow");
   transitcount--;
   if (transitcount) return;
   enqmvcpu(&migratetransitcountzeroqueue);
} /* End decrementtransitcount */
 
 
/*********************************************************************
pdrintegritygood - Check integrity data in PDR
 
  Input -
     cte     - Pointer to the CTE of the PDR page
 
  Output -
     Pointer to the PDR (in IOSYSWINDOW) if the PDR is good, else NULL
*********************************************************************/
static PDR *pdrintegritygood(CTE *cte)
{
   PDR *pdr = (PDR*)map_window(IOSYSWINDOW, cte, MAP_WINDOW_RW);
 
   if (pdr->checkbyte
         != ((pdr->pd.integrity & PDINTEGRITYCOUNTER)
             | (~pdr->precheck & (UCHAR_MAX - PDINTEGRITYCOUNTER))) )
      return 0;
   return pdr;
} /* End pdrintegritygood */
 
 
/*********************************************************************
gupdint - Update the integrity data in a PDR
 
  Input -
     pdr     - Pointer to the PDR
 
  Output - None
*********************************************************************/
static void gupdint(PDR *pdr)
{
   pdr->pd.integrity++;
   pdr->checkbyte =
            (pdr->pd.integrity & PDINTEGRITYCOUNTER)
             | (~pdr->precheck & (UCHAR_MAX - PDINTEGRITYCOUNTER));
} /* End gupdint */
 
 
/*********************************************************************
pdrwriteended - Process end of a write of the PDR
 
  Input -
     req     - Pointer to the request that ended
 
  Output - None
*********************************************************************/
static void pdrwriteended(REQUEST *req)
{
   DEVREQ *drq = req->devreqs;
 
   if (DEVREQCOMPLETE != drq->status) {
      gcodismt(drq->device);               /* Can't write, dismount */
   }
   if (req->pagecte) gspmpfa(req->pagecte); /* Free any gotten page */
   getunlok((uint32)drq->device);             /* Unlock location */
   getredrq(drq);                           /* Free the devreq */
   getrereq(req);                           /* Free the request */
   decrementtransitcount();                 /* Test to run waiters */
}  /* End pdrwriteended */
 
 
/*********************************************************************
pdrreadended - Process end of a write of the PDR
 
  Input -
     req     - Pointer to the request that ended
 
  Output - None
*********************************************************************/
static void pdrreadended(REQUEST *req)
{
   DEVREQ *drq = req->devreqs;
   PDR *pdr;
 
   if  (DEVREQCOMPLETE != drq->status
        || !(pdr = pdrintegritygood(req->pagecte))) {
      gcodismt(drq->device);               /* Can't read, dismount */
      if (req->pagecte) gspmpfa(req->pagecte); /* If gotten free page */
      getunlok((uint32)drq->device);           /* Unlock location */
      getredrq(drq);                           /* Free the devreq */
      getrereq(req);                           /* Free the request */
      decrementtransitcount();                 /* Test to run waiters */
      return;
   }
   req->pagecte->ctefmt = PDRFrame;
   grtuppdr(drq->device, pdr);        /* Update the PDR */
   gupdint(pdr);            /* Update the integrity data in the PDR */
   req->type = REQMIGRATEWRITE;       /* Change request to a write */
   req->pcfa.flags = 0;
   drq->status = DEVREQOFFQUEUE;
   req->doneproc = pdrwriteended;
   req->completioncount = 1;
   if (!pdrupdateactive) gddenq(req);
}  /* End pdrreadended */
 
 
/*********************************************************************
gupdpdr - Update a pack descriptor record (PDR)
 
  Input -
     actor   - Pointer to the domain root of the actor
 
  Output -
     Returns zero if PDR not updated and actor queued, else non-zero
*********************************************************************/
int gupdpdr(NODE *actor)
{
   DEVICE *(*pdrupdatedevice)(void);
 
   pdrupdatedevice = grtfpud;
   for (;;) {                     /* Do forever */
      REQUEST *req = acquirerequest();
      DEVICE *dev;
      DEVREQ *drq;
 
      if (!req) {
         enqueuedom(actor, &noiorequestblocksqueue);
         return 0;
      }
      dev = (*pdrupdatedevice)();
      if (!dev) {
         getrereq(req);                   /* Return request */
   break;
      }
      pdrupdatedevice = grtnpud;
      if (!getlock((uint32)dev, actor)) {
         getrereq(req);                   /* Return request */
         return 0;
      }
         /* Build REQUEST to read the pack descriptor record */
      drq = acquiredevreq(req);
      if (!drq) {
         getunlok((uint32)dev);           /* Release lock */
         getrereq(req);                   /* Return request */
         enqueuedom(actor, &noiorequestblocksqueue);
         return 0;
      }
      drq->device = dev;
      drq->offset = dev->pdraddress;
      md_dskdevreqaddr(drq);
      req->doneproc = pdrreadended;
      req->doneparm = 0;
      req->potaddress.range = 0;
      req->potaddress.offset = 0;
      req->completioncount = 1;
      req->pagecte = NULL;
      /* REQCDA not used. */
      req->pcfa.flags = 0;              /* clear REQPOT */
      req->type = REQMIGRATEREAD;
 
      pdrupdateactive = 1;           /* Mark domain level active */
      transitcount++;
      gddenq(req);
      if (REQMIGRATEWRITE == req->type) {   /* Synchronous finish */
         gddenq(req);                       /* Start write part of op */
      }
      pdrupdateactive = 0;
   }
   if (transitcount) {            /* Need to wait for I/O */
      enqueuedom(actor, &migratetransitcountzeroqueue);
      return 0;
   }
   return 1;
}  /* End gupdpdr */
