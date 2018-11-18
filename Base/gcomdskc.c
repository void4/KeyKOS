/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* gcomdskc.c - I/O SUBROUTINES COMMON BETWEEN GDSKDVR AND GINTDSK */
#include "sysdefs.h"
#include "ioreqsh.h"
#include "gdskdvrh.h"
#include "ioworkh.h"
#include "sysgenh.h"
#include "mi2mdioh.h"
#include "consmdh.h"

void enqueue_devreq(DEVREQ *drq)
/* Enqueue a devreq on its device. */
/* drq->status must be DEVREQOFFQUEUE */
{
   PHYSDEV *pdev = drq->device->physdev;

   if (pdev->enqstate == DEVNOTREADY)
      drq->status = DEVREQNODEVICE;
   else {
      /* Chain to end of device queue. */
      DEVREQ *last = pdev->ioqlast;
      drq->nextio = (DEVREQ *)pdev;
      drq->previo = last;
      if (last == (DEVREQ *)pdev)
           pdev->ioqfirst = drq; /* The only one. */
      else last->nextio = drq;
      pdev->ioqlast = drq;
      drq->status = DEVREQPENDING;
   }
}

void enqueue_devreqs(
   REQUEST *req)
/* Enqueue all OFFQUEUE devreqs. */
{
   DEVREQ *drq;

   /* loop over all DEVREQs */
   for (drq=req->devreqs; drq; drq=drq->devreq) {
      if (drq->status == DEVREQOFFQUEUE) {
         enqueue_devreq(drq); /* enqueue the DEVREQ */
      }
   } /* end of loop over all DEVREQs */
}

REQUEST *enqrequestworkqueue = NULL;
void gcoreenq(void)
/* Reconsider the requests that are on enqrequestworkqueue. */
/* The purpose of enqueueing these requests is to limit recursion. */
{
   while (enqrequestworkqueue) {
      REQUEST *req = enqrequestworkqueue;
      enqrequestworkqueue = req->next;  /* dequeue */
      gddreenq(req);
   }
}

REQUEST *nopagesrequestqueue = NULL;
/* All requests on this queue are read requests
   and have completioncount nonzero.
   None of their devreqs are PENDING or selected. */
void serve_nopagesrequestqueue(void)
/* Caller must call gcoreenq. */
{
   /* Serve the requests by adding to enqrequestworkqueue. */
   while (nopagesrequestqueue) {
      REQUEST *req = nopagesrequestqueue;
      nopagesrequestqueue = req->next;  /* remove from nopages queue */
      enqueue_devreqs(req);
      req->next = enqrequestworkqueue;  /* add to work queue */
      enqrequestworkqueue = req;
   }
}

static REQUEST *completedrequestqueue = NULL;
bool rcractive = FALSE;
void return_completed_requests(void)
/* Serves the queue of completed requests.
   This routine is called at the "end" of any processing that
   might need it.
   Its purpose is to eliminate the unlimited recursion that
   could occur when an ending proc calls for further I/O. */
{
   REQUEST *req;

   if (rcractive) return;  /* Already active, no need to recurse further */
   rcractive = TRUE;
   while (req =/* assignment */ completedrequestqueue) {
      completedrequestqueue = req->next; /* Remove from queue */
      req->onqueue = 0;
      req->next = NULL;
      requestscompletedbytype[(int)req->type]++; /* Count requests done */
      (*req->doneproc)(req);  /* Call the completion proc */
   }
   rcractive = FALSE;
}

#define xoffqueue 1  /* some devreqs are off queue */
#define xselected 2  /* some devreqs are selected */
#define xpending  4  /* some devreqs are pending */
#define xcountnz  8  /* completioncount is nonzero */
static const unsigned char completionstate[8] = {
   xoffqueue,  /* DEVREQOFFQUEUE */
   xpending,   /* DEVREQPENDING */
   xselected,  /* DEVREQSELECTED */
   0,          /* DEVREQCOMPLETE */
   0,          /* DEVREQPERMERROR */
   0,          /* DEVREQNODEVICE */
   0,          /* DEVREQABORTED */
   xselected}; /* DEVREQABORTOREND */

void check_request_done(REQUEST *req)
/* Checks if the request has completed. */
/* Caller must eventually call return_completed_requests. */
/* Caller may also need to call gcoreenq. */
{
   int cumstate;
   DEVREQ *drq;

   if (req->onqueue) return;   /* It is already on some queue
        and will be taken care of when that queue is served. */

   /* Here we calculate three bits that tell whether any devreqs
      are offqueue, pending, or selected. */
   cumstate = 0;
   for (drq=req->devreqs; drq; drq=drq->devreq)
      cumstate |= completionstate[(int)drq->status];
   if (req->completioncount) cumstate |= xcountnz;
   switch (cumstate) {
    default: crash("GCO236 cumstate out of range");
    case xpending+xselected+xoffqueue:
    case xpending+xselected:
    case xpending+xoffqueue:
    case xpending:
      crash("GCO237 pending devreqs with count zero");

    case xcountnz+xselected+xoffqueue:
      if (   req->type == REQJOURNALIZEWRITE
          || req->type == REQHOMENODEPOTWRITE)
         return; /* Dont reenqueue while one is selected. */
      /* else fall into case below to reenqueue. */
    case xcountnz+xpending+xselected+xoffqueue:
    case xcountnz+xpending+xoffqueue:
    case xcountnz+xoffqueue:
      /* This request has devreqs off queue that need to be restarted. */
      enqueue_devreqs(req);
      /* Reconsider it later. */
      req->next = enqrequestworkqueue;
      enqrequestworkqueue = req;
      req->onqueue = 1;
    case xcountnz+xpending+xselected:
    case xcountnz+xselected:
    case xselected+xoffqueue:
    case xselected:
    case xcountnz+xpending:
      return;

    case xoffqueue:/* Done because completioncount is zero and
                      no devreqs are selected. */
    case xcountnz: /* Done even though completioncount is nonzero,
                      because there are no devreqs we can run. */
    case 0:        /* Done. All DEVREQs Perm Error, Complete,
                      No Device, or Aborted. */
      /* Request is done. Add it to the completedrequestqueue. */
      req->next = completedrequestqueue;
      completedrequestqueue = req;
      req->onqueue = 1;
      return;
   }
} /* end of check_request_done */

void gcodidnt(DEVREQ *drq)
{
   REQUEST *req = drq->request;

   switch (drq->status) {
    case DEVREQSELECTED:
      drq->status = DEVREQOFFQUEUE;
      break;
    case DEVREQABORTOREND:
      drq->status = DEVREQABORTED;
      break;
    default:
      crash("gcodidnt unexpected status");
   }
   req->completioncount++;
   check_request_done(req);
}

void gcodismt(struct Device *dev)  /* Dismount device */
/* Caller must ensure there is no I/O active on the device. */
{
   grtclear(dev);
} /* end of gcodismt */

void unlinkdevreq(DEVREQ *drq)
{
   register DEVREQ *next = drq->nextio,
                   *prev = drq->previo;
   register PHYSDEV *pdev = drq->device->physdev;

   if (next == (DEVREQ *)pdev)
        pdev->ioqlast = prev;
   else next->previo = prev;
   if (prev == (DEVREQ *)pdev)
        pdev->ioqfirst = next;
   else prev->nextio = next;
   drq->nextio = drq->previo = drq; /* chain to itself */
}

void gcodismtphys(
   PHYSDEV *pdev)
/* There must be no I/O active on the device. */
{
   LOGDISK *ldev;
   DEVREQ *drq;

   consprint("Disk went offline.\n");
   /* Dismount all the logical disks on this physical device. */
   for (ldev=ldev1st; ldev<ldevlast; ldev++) {
      if (ldev->physdev == pdev) gcodismt(ldev);
   }
   /* Clean out the queued devreqs. */
   for (;;) {
      drq = pdev->ioqfirst;
      if (drq == (DEVREQ *)pdev)
   break;
      unlinkdevreq(drq);
      drq->status = DEVREQNODEVICE;
      check_request_done(drq->request);
   }
   pdev->enqstate = DEVNOTREADY;
}

void dequeue_pending_devreqs(
   REQUEST *req)
/* Dequeues any pending devreqs for this request. */
{
   register DEVREQ *d;
   for (d = req->devreqs; d; d = d->devreq) {
      if (d->status == DEVREQPENDING) {
         d->status = DEVREQOFFQUEUE;
         unlinkdevreq(d);
      }
   }
}

void select_devreq(
   DEVREQ *drq)
/* Select a DEVREQ for processing. */
/* Maintains the following assertions:
   1. req->completioncount != 0
      || no devreqs are DEVREQPENDING
   2. (req->type != REQJOURNALIZEWRITE
       && req->type != REQHOMENODEPOTWRITE)
      || no devreqs are selected (including ABORTOREND)
      || no devreqs are DEVREQPENDING
 */
{
   if (drq->status != DEVREQPENDING)
      crash("select_devreq unexpected status");
   unlinkdevreq(drq);
   drq->status = DEVREQSELECTED;
   {  register REQUEST *req = drq->request;

      if (req->completioncount == 0)
         crash("GCO234 Selecting devreq while count 0");
      req->completioncount--;
      if (req->completioncount == 0
          || req->type == REQJOURNALIZEWRITE
          || req->type == REQHOMENODEPOTWRITE)
         /* Don't do any other devreqs now. */
         dequeue_pending_devreqs(req);
   }
}

