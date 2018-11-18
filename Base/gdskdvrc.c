/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* gdskdvrc.c - ENQUEUE REQUESTS AND PERFORM DISK I/O */
// #include <string.h>
#include <stdio.h>
#include "sysdefs.h"
#include "ioreqsh.h"
#include "devmdh.h"
#include "gcoh.h"
#include "gccwh.h"
#include "gcleanlh.h"
#include "dskiomdh.h"
#include "wsh.h"
#include "gswapah.h"
#include "mi2mdioh.h"
#include "gmigrath.h"
#include "sysgenh.h"
#include "queuesh.h"
#include "gdskdvrh.h"
#include "gintdskh.h"
#include "ioworkh.h"
#include "scafoldh.h"
#include "consmdh.h"
#include "memutil.h"

#define armposition(drq) ((drq)->offset*BLOCKSPERPAGE \
             + (drq)->device->physoffset )

static void startonedisk(PHYSDEV *pdev)
/* Start I/O on a disk (or queue it to start later) */
{
   if (pdev->enqstate != DEVRUNNINGQUEUE)
      crash("GDD006 Device not reserved");
   pdev->pd_dynamic_sp->pkt_comp = diskdone_intproc;
   pdev->pd_dynamic_sp->pkt_private = (opaque_t)pdev;
   pdev->pd_dynamic_sp->pkt_flags = FLAG_NOPARITY | FLAG_NODISCON;
   start_scsi_cmd_sense(pdev->pd_dynamic_sp);
}

enum ccwsret start_devreq(
   DEVREQ *drq)
/* Caller must call checkforcleanstart. */
/* If NOPAGES returned, caller must call restart_disk(drq->device->physdev). */
{
   enum ccwsret ret;

if(lowcoreflags.iologenable){char str[80];
// sprintf(str, "starting devreq %x\n", drq);
 logstr(str);}
   select_devreq(drq);
   ret = build_ccws(drq);
   switch (ret) {
    case NOPAGES:
      /* There is no page frame available to satisfy this (read) request. */
      if (!(iosystemflags & WAITFORCLEAN))
         crash("No page available but not waiting for clean");
      /* Take the request off queue, enqueue on nopagesrequestqueue. */
      {  REQUEST *req = drq->request;
         if (req->onqueue) { /* already on a queue */
            REQUEST **r = &enqrequestworkqueue; /* only queue it could be on */
            for (; *r != req; r=&((*r)->next))
               if (!*r) crash("GDD732 not on enqreqworkqueue");
            *r = req->next;  /* dequeue */
         }
         req->next = nopagesrequestqueue;
         nopagesrequestqueue = req;
         req->onqueue = 1;
         gcodidnt(drq);
         dequeue_pending_devreqs(req); /* These won't run either. */
      }
      break;
    case NOSWAPSPACE:
      {  REQUEST *req = drq->request;
         drq->status = DEVREQNODEVICE;
         req->completioncount++;
         check_request_done(req);
      }
      break;
    case CCWSBUILT:
      drq->device->physdev->doneproc = ginioida;
      startonedisk(drq->device->physdev);
      break;
   } /* end of switch */
   return ret;
} /* end of start_devreq */

void restart_disk(
   PHYSDEV *pdev)
/* See if there is anything to do on this disk */
/* Called when this disk has become idle. */
/* Caller must call checkforcleanstart. */
{
   DEVREQ *drq;

restartdisktrynext:

   pdev->enqstate = DEVSTART;

   /* Find best devreq to run next on this disk. */

   drq = NULL;  /* has best so far */
   {  DEVREQ *testdrq;
      for (testdrq=pdev->ioqfirst;
           testdrq != (DEVREQ *)pdev;
           testdrq=testdrq->nextio) {
         if (drq == NULL)
            drq = testdrq; /* Only is best */
         else {
#define priorityof(drq) (prioritytable[(int)((drq)->request->type)])
/* N.B. in prioritytable, lower numbers represent higher priority. */
            int testpri = priorityof(testdrq);
            int oldpri = priorityof(drq);
            if (testpri < oldpri)
               drq = testdrq; /* testdrq has higher priority */
            else if (testpri == oldpri) { /* if same priority */
               /* Calculate distance of each devreq from the
                  current position of the disk. */
               /* N.B. The distances are declared unsigned(!)
                  to implement the rule that we always prefer to
                  move the disk to higher addresses.
                  This rule prevents starvation of devreqs. */
               unsigned long testdist = armposition(testdrq)
                     - pdev->lastaddress;
               unsigned long olddist = armposition(drq)
                     - pdev->lastaddress;
               if (testdist < olddist)
                  drq = testdrq; /* new one is closer */
            }
         }
      }
   }
   /* drq now has best devreq (if any) */

   if (drq) { /* A devreq to run */
      switch (start_devreq(drq)) {
       default: goto restartdisktrynext;
       case CCWSBUILT:
         return; /* The disk is running now. */
      }
   }
   /* No devreqs to run on this device.
      See if any cleaning to do. */
   {
      LOGDISK *ldev;
      for (ldev=ldev1st; ldev<ldevlast; ldev++) {
         if (ldev->physdev == pdev   /* on this disk */
             && ldev->flags & DEVCURRENTSWAPAREAHERE) {
            if (gclbuild(ldev)) { /* cleaning was built */
               pdev->doneproc = ginioicp;
               startonedisk(pdev);
               return; /* The disk is running now. */
            }
            else ; /* Could not build for cleaning. */
               /* "The call in IOINTER to GETCFCS will re-try the 
                  clean later" */
         }
      }
   }
} /* end of restart_disk */

void gddreenq(REQUEST *req)
/* Re-enqueue a request */
/* Checks if any of its devreqs can be started. */
{
   DEVREQ *drq;

   req->onqueue = 0; /* not on any queue */
   for (;;) { /* Start as many DEVREQs as possible */
      DEVREQ *bestdrq = NULL;
      /* Find the best DEVREQ to start */
      for (drq=req->devreqs; drq; drq=drq->devreq) {
         PHYSDEV *dev = drq->device->physdev;
         if (drq->status == DEVREQPENDING
             && dev->enqstate <= DEVRUNNINGADD) {
            /* We can start this DEVREQ. See if it is best. */
            if (bestdrq == NULL) bestdrq = drq; /* Only, therefore best. */
            else {
               register PHYSDEV *bestdev = bestdrq->device->physdev;
#define labs(j)  ((j)<0?-(j):(j))
#define seekdist(drq, pdev) labs(armposition(drq) \
       - pdev->lastaddress)
               if (dev->enqstate < bestdev->enqstate
                           /* Lower enqstate is better. */
                   || seekdist(drq, dev) < seekdist(bestdrq, bestdev)
                           /* lower seek distance is better */ )
                  bestdrq = drq;
            }
         }
      }
      if (bestdrq == NULL) { /* no more DEVREQs to start */
         check_request_done(req);
         return_completed_requests();
         return;
      }
      /* Start bestdrq. */
      switch (start_devreq(bestdrq)) {
       case NOPAGES:
         /* The request is dead for now. */
         restart_disk(bestdrq->device->physdev);
         return_completed_requests();
         return;
       case NOSWAPSPACE:
         enqueue_devreqs(req); /* try another device */
         break;
       case CCWSBUILT:
         break;
      } /* end of switch */
   } /* end of start as many DEVREQs as possible */
} /* end of gddreenq */

void gddenq(REQUEST *req)  /* Enqueue a request */
{
   DEVREQ *drq;

   /* req->enqtime = read_system_timer(); */
   if (req->completioncount == 0
       || req->pcfa.flags & REQPAGEALLOCATED)
      crash("GDD132 New request has bad format");
   /* loop over all DEVREQs */
   for (drq=req->devreqs; drq; drq=drq->devreq) {
      if (drq->status != DEVREQOFFQUEUE)
         crash("GDD133 New request has devreq not OFFQUEUE");
      if (drq->request != req)
         crash("GDD134 New request has devreq->request wrong");
      enqueue_devreq(drq); /* enqueue the DEVREQ */
   } /* end of loop over all DEVREQs */
   gddreenq(req);
}

void gddabtdr(
   DEVREQ *drq)
/* Abort a devreq. */
/* On output,
      Either the DEVREQ is aborted (and the I/O never takes place)
      or the I/O takes (or took) place before any I/O (including
      cleaning) to that disk location that is initiated after
      gddabtdr returns.
 */
{
   switch (drq->status) {
    case DEVREQPENDING:
      unlinkdevreq(drq); /* Remove from device queue. */
      /* fall into next case */
    case DEVREQOFFQUEUE:
      drq->status = DEVREQABORTED;
      check_request_done(drq->request);
      gcoreenq();
      return_completed_requests();
      break;
    case DEVREQSELECTED:
      drq->status = DEVREQABORTOREND;
      break;
    case DEVREQCOMPLETE:
    case DEVREQPERMERROR:
    case DEVREQNODEVICE:
    case DEVREQABORTED:
    case DEVREQABORTOREND:
      break;
   }
} /* end of gddabtdr */

static unsigned char seedid[8] = "\0\0\0\0\0\0\0\0";
bool gdddovv(    /* Do volume verification, mount disk */
   PHYSDEV *pdev,
   unsigned long physoffset,
   unsigned long nblks,
   PDR *pdr)
/* Returns TRUE iff disk was mounted. */
/* If FALSE returned, caller must call gcodismt */
{
   /* Check the check byte. */
   if (pdr->checkbyte
       != ((pdr->pd.integrity & PDINTEGRITYCOUNTER)
           | ((pdr->precheck
               ^ 0xff) & ~PDINTEGRITYCOUNTER) )
      ) return FALSE;
   if (pdr->pd.version != 0) return FALSE;
   if (Memcmp(seedid, "\0\0\0\0\0\0\0\0", 8) == 0)
      /* First pack mounted */
      Memcpy(seedid, pdr->pd.seedid, 8);
   else if (Memcmp(seedid, pdr->pd.seedid, 8) != 0)
      return FALSE; /* Wrong seedid */
   /* Check that pack is unique. */
   {
      LOGDISK *ldev;
      for (ldev=ldev1st; ldev<ldevlast; ldev++) {
         if (Memcmp(pdr->pd.packserial, ldev->packid, 8) == 0)
            crash("GDDDOVV001 Duplicate packid");
      }
   }
   /* Allocate a logical device. */
   if (ldevlast >= ldev1st+MAXDEVICES) {
      crash("Out of LOGDISK blocks.");
      return FALSE;
   }
   else {
      LOGDISK *ldev = ldevlast++;
      /* Add all ranges. */
      {
         PDRD *pdrd = pdr->ranges;
         int i;
         for (i=pdr->pd.rangecount; i>0; --i) {
            if (grtadd(pdrd, ldev) == 0) {
               /***... deallocate logical device block */
               return FALSE;
            }
            pdrd++;
         }
         enqmvcpu(&rangeunavailablequeue);
      }
      Memcpy(ldev->packid, pdr->pd.packserial, 8);
      ldev->flags = DEVMOUNTED;
      ldev->physdev = pdev;
      ldev->physoffset = physoffset;
      ldev->extent = nblks/8;
      ldev->pdraddress = PDRPAGE; /* Is this used? */
      consprint("KeyKOS disk mounted.\n");
      return TRUE;
   }
} /* end of gdddovv */

int gddstartpageclean(void)
/* Returns zero if nothing cleaned */
{
   int retval = 0;
   DEVICE *ldev;

   iosystemflags |= PAGECLEANNEEDED;
   ldev = gswfbest(DEVSTART);
   while (ldev) {
      if (ldev->physdev->enqstate != DEVSTART)
         crash("GDD005 gswxbest returned bad device");
      /* Start a page clean on this device. */
      if (gclbuild(ldev)) { /* if something built */
         retval = 1;  /* something built */
         ldev->physdev->doneproc = ginioicp;
         startonedisk(ldev->physdev);
      }
      if (!(iosystemflags & PAGECLEANNEEDED)) break; /* cleaned enough */
      ldev = gswnbest(DEVSTART);
   }
   return_completed_requests();
   return retval;
} /* End of gddstartpageclean */

