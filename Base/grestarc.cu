/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  GRESTARC - Code to restart from a checkpoint - KeyTech Disk I/O */
 
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include "lli.h"
#include "sysdefs.h"
#include "primmeth.h"
#include "cal2tod.h"
#include "timemdh.h"
#include "keyh.h"
#include "prepkeyh.h"
#include "queuesh.h"
#include "spaceh.h"
#include "wsh.h"
#include "kschedh.h"
#include "geteh.h"
#include "ioworkh.h"
#include "ioreqsh.h"
#include "ckpthdrh.h"
#include "dskiomdh.h"
#include "devmdh.h"
#include "gdi2greh.h"
#include "getih.h"
#include "grangeth.h"
#include "grt2greh.h"
#include "memomdh.h"
#include "kerinith.h"
#include "jpageh.h"
#include "gdi2geth.h"
#include "gdirecth.h"
#include "locksh.h"
#include "kermap.h" /* for lowcoreflags */
#include "kernelp.h"
#include "consmdh.h"
 
#if LATER
#define NUMBERPIXELPAGES 64
char pixelread[NUMBERPIXELPAGES] = {0}; /* 1 if pixel page has been read */
#endif
uint64 grestarttod;           /* Restart TOD for the journal page */
 
/* Local static constants */
 
static CDA cdaone = {0, 0, 0, 0, 0, 1};
static uchar cdaofprimemeter[6] = {0x80,0,primemetercda>>24 & 0xff,
                                          primemetercda>>16 & 0xff,
                                          primemetercda>>8 & 0xff,
                                          primemetercda & 0xff};
 
 
/* Local static variables */
 
static char synciolock = 0;   /* Flag for sync I/O finish */
static char journalpageset = 0;   /* Journal page already set up */
static char donesomething= 0;     /* In readprocessnodepots/readnode */
static char moretodo = 0;         /* In readprocessnodepots/readnode */
static CTE *header[2];            /* CTEs of 1st and 2nd headers */
static int headerreadsinprogress = 2; /* Number header reads to go */
static int ioinprogress = 0;      /* I/O operations in progress */
static uint64 hdrtod;                /* Ckpthdr TOD for journal page */
static CTE *activecte = NULL;     /* CTE of current header */
static CTE *nextheadercte= NULL;  /* CTE of next header to process */
static unsigned int headerindex = 0;       /* Next entry in current header */
static void (*restartendingproc)(void); /* Proc to call after restart */
 
 
/* Prototypes of internal routines */
 
static void readdiskdirectories(void);
static void readprocessnodepots(void);
 
extern struct KernelPage *kernelpagept; 
 
 
/*********************************************************************
delta_calclock2tod - Calculate difference between now and a CalClock
                     value and return the difference in system timer
                     units
 
  Input -
     oldtime - The older of the two times
 
  Output -
     The difference in system timer units. Zero if now is before then
     or if the differences exceeds the range of system timer units
*********************************************************************/
static uint64 delta_calclock2tod(struct CalClock oldtime)
{
   struct CalClock now;
   uint64 nt, ot;
   uint16 retcode;
 
   now = read_calendar_clock();
   retcode = cal2tod(oldtime.value, &ot);
   if (retcode) {
      return 0;
   }
   retcode = cal2tod(now.value, &nt);
   if (retcode) {
      return 0;
   }
   if (nt < ot) return 0;
   nt -= ot;
   return nt;
} /* End delta_calclock2tod */
 
 
/*********************************************************************
initializereadended - Ending proc for journal page or nodepot read
 
  Input -
     req     - Pointer to the request that ended
 
  Output - None
 
  Notes:
     This routine has a maximum recursion depth of 2 in the case where
     there are both synchronous and asychronous disk reads in the same
     system.
*********************************************************************/
static void initializereadended(REQUEST *req)
{
   getended(req);
   if (!(ioinprogress--))         /* Decrement I/O in progress count */
       crash("GRESTARC009 ioinprogress underflow");
   if (!synciolock) readprocessnodepots(); /* Continue reading */
} /* End initializereadended */


#if LATER
static void pixelreadended(REQUEST *req)
{
   if (req->pcfa.flags & REQPOT) {  /* Got the alloc pot */
      getended(req);
   } else { /* got the pixel buffer */
      CTE *cte = req->pagecte;

      if (getendedcleanup(req) == 1) {  /* I/O successful */
         pixelread[b2int(cte->use.page.cda+2,4)] = 1;
         getredrq(req->devreqs);
         if (!(cte->extensionflags & ctkernellock))
            crash("GETC008 Page CTE not kernel locked ");
         cte->extensionflags &= ~ctkernellock;  /* Unlock cte */
      }
      getrereq(req);
   }
   if (!(ioinprogress--))         /* Decrement I/O in progress count */
       crash("GRESTARC209 ioinprogress underflow");
   if (!synciolock) readprocessnodepots(); /* Continue reading */
} /* End pixelreadended */
#endif
 
 
/*********************************************************************
readnode - Find node in core or read it from disk
 
  Input -
     cda     - Pointer to the CDA to read, high bit on
 
  Output -
     A pointer to the NODE frame or NULL
*********************************************************************/
static NODE *readnode(uchar *cda)
{
   CDA parmcda;
   NODE *nf;
   struct CodeIOret ior;
 
   Memcpy(parmcda, cda, sizeof(CDA));
   parmcda[0] &= 0x7f;    /* Get CDA with high bit off */
   nf = srchnode(parmcda);
   if (nf) return nf;
       /* Node is not in a frame */
   synciolock = 1;
   ioinprogress++;
   ior = getreqn(cda, REQNORMALPAGEREAD, initializereadended, NULL);
   synciolock = 0;
   switch (ior.code) {
    case io_notmounted:
      crash("GRESTARC016 Required node not mounted at restart");
    case io_notreadable:
      crash("GRESTARC017 Required node not readable at restart");
    case io_started:
      moretodo = 1;
      donesomething = 1;
      break;     /* Look again incase synchronous read */
    case io_potincore:
      nf = movenodetoframe(cda, ior.ioret.cte);
      if (!nf)
         crash("GRESTARC018 Not enough node space for restart");
      break;
    case io_cdalocked:
    case io_noioreqblocks:
      moretodo = 1;
      ioinprogress--;
      break;
    default:
      crash("GRESTARC005 Bad return code from getreqn");
   }  /* End switch on results of getreqn call */
   return nf;
} /* End readnode */
 
#if LATER
/*********************************************************************
readpixelpage - Read pixel buffer from disk
 
  Input -
     cda     - Pointer to the CDA to read
 
  Output -
     A pointer to the CTE or NULL
*********************************************************************/
static bool readpixelpage(
   int index)
/* returns TRUE if read started. */
{
   CDA pcda = {0x7f,0xff,0,0,0,00};
   CTE *cte;
   struct CodeIOret ior;
 
   int2b(index, pcda+2,4); /* set cda */
   cte = srchpage(pcda);
   if (!cte) crash("GRE442 pixel cte not found");
   synciolock = 1;
   ioinprogress++;

   ior = gdilook(pcda, NULL); /* find page on disk */
   getreqpcommon(&ior, pcda, REQNORMALPAGEREAD, pixelreadended, NULL, cte);
   synciolock = 0;
   switch (ior.code) {
    case io_notmounted:
      crash("GRESTARC216 Pixel page not mounted at restart");
    case io_notreadable:
      crash("GRESTARC217 Pixel page not readable at restart");
    case io_pagezero:
      ioinprogress--;
      clear_page(cte);
      pixelread[index] = 1;
      return TRUE;
    case io_started:
      moretodo = 1;
      donesomething = 1;
      break;     /* Look again incase synchronous read */
    case io_cdalocked:
    case io_noioreqblocks:
      moretodo = 1;
      ioinprogress--;
      return FALSE;
    default:
      crash("GRESTARC205 Bad return code from getreqpcommon");
   }  /* End switch on results of getreqn call */
   return FALSE;
} /* End readnode */
#endif
 
/*********************************************************************
readprocessnodepots - Read nodepots with nodes with processes in them
 
  Input - None
 
  Output - None
*********************************************************************/
static void readprocessnodepots(void)
{
   PCFA pcfa;
   NODE *nf;
   uchar *cda;
 
   for (;;) {             /* Do as long as each loop does something */
      donesomething = 0;
      moretodo = 0;
      if (!journalpageset) {
         CTE *cte = srchpage(cdaone); /* Look for journal page */
 
	 /* If we are doing checkpoint kernel restart, make sure we have not
	    already read in the Journal page in main(). */
	 if (kernelpagept)
	    crash("GRESTARC Journal page(kernelpagept) already set");

         if (!cte) {                  /* Journal page not in core */
            struct CodeIOret ior;
 
            synciolock = 1;
            ioinprogress++;
            ior = getreqp(cdaone, REQNORMALPAGEREAD,
                          initializereadended, NULL);
            synciolock = 0;          /* Don't need to read */
            switch (ior.code) {
             case io_notmounted:
               crash("GRESTARC010 Journal page not mounted");
             case io_notreadable:
               crash("GRESTARC011 Journal page not readable");
             case io_pagezero:
               ioinprogress--;
               pcfa = *ior.ioret.pcfa;      /* Save the PCFA */
               cte = gspgpage();            /* Get a page frame */
               if (!cte) crash("GRESTARC012 No frame for journal page");
               setupvirtualzeropage(&pcfa, cte);
               break;                  /* cte is set up */
             case io_started:
               moretodo = 1;
               donesomething = 1;
               break;                  /* Leave cte == NULL */
             case io_cdalocked:
               moretodo = 1;
               ioinprogress--;
               break;                  /* Leave cte == NULL */
             case io_noioreqblocks:
               moretodo = 1;
               ioinprogress--;
               return;
             default:
               crash(
               "GRESTARC005 Bad return code from getreqp"
                      );
            }
         }
 
         if (cte) {    /* cte points to the CTE of the journal page */
	 
	    corelock_page(cte);
	    cte->flags |= ctchanged;
	    kernelpagept = (struct KernelPage *)
                       map_window(KERNELWINDOW, cte, MAP_WINDOW_RW);
            kernelpagept->KP_LastCheckPointTOD = hdrtod;
            kernelpagept->KP_RestartCheckPointTOD = hdrtod;
            kernelpagept->KP_RestartTOD = grestarttod;
            journalpageset = 1;
         }
      }
      nf = readnode(cdaofprimemeter);
      if (!nf) return;
      nf->corelock = 1;      /* Lock prime meter in core */

#if LATER
      /* Read pixel buffer pages. */

      {  int i;
         for (i=0; i < NUMBERPIXELPAGES; i++) {
            if (lowcoreflags.pixelfetchdisable)
               pixelread[i]=1; /* disable fetching pixels to
                       save any debugging info on the screen */
            if (!(pixelread[i])) {
               if (!readpixelpage(i))
                  return;
            }
         }
      }
#endif
      idiirap();             /* Initialize read of active processes */
      for (cda = idinap(); cda; cda = idinap()) {
         nf = readnode(cda);
         if (!nf) return;
         nf->flags |= NFDIRTY;
         enqueuedom(nf, &frozencpuqueue);
      }
      if (!donesomething) {              /* Didn't do anything */
         if (!moretodo) {                /* No more to do */
            enqmvcpu(&frozencpuqueue);     /* Start running processes */
            (*restartendingproc)();        /* Run the system */
         }
         return;
      }
   }           /* Loop back if we did anything this time through */
} /* End readprocessnodepots */
 
 
/*********************************************************************
freedevreqs - Free all the devreqs associated with a request
 
  Input -
     req     - Pointer to the request that finished
 
  Output - None
*********************************************************************/
static void freedevreqs(REQUEST *req)
{
   DEVREQ *drq;
 
   for (drq = req->devreqs; drq; drq = req->devreqs) {
      req->devreqs = drq->devreq;
      getredrq(drq);
   }
} /* End freedevreqs */
 
 
/*********************************************************************
getcompleteddevreqpointer - Arrange to free the first complete devreq
 
  Input -
     req     - Pointer to the request that finished
 
  Output -
     Pointer to pointer to the first devreq marked DEVREQCOMPLETE
*********************************************************************/
static DEVREQ **getcompleteddevreqpointer(REQUEST *req)
{
   DEVREQ **drqp = &req->devreqs;
   DEVREQ *drq;
 
   for (drq = *drqp; drq; (drqp = &drq->devreq, drq = *drqp)) {
      if (DEVREQCOMPLETE == drq->status)
   return drqp;
   }
   return NULL;
} /* End getcompleteddevreqpointer */
 
 
/*********************************************************************
diskdirectoryreadended - Called after a disk directory read finishes
 
  Input -
     req     - Pointer to the request that finished
 
  Output - None
*********************************************************************/
static void diskdirectoryreadended(REQUEST *req)
{
   CTE *cte;
   uchar *p;
   DEVREQ **drqp = getcompleteddevreqpointer(req);
 
   if (!drqp)
      crash("GRESTARC008 Unable to read a disk directory block");
   cte = req->pagecte;
   p = map_window(IOSYSWINDOW, cte, MAP_WINDOW_RO);
   if (!idibde(p, hdrtod)) {       /* Process disk directory page */
      DEVREQ *drq = *drqp;         /* Format error in entry - try next */

      *drqp = drq->devreq;         /* Dequeue devreq for bad entry */
      if (!req->devreqs)
         crash("GRESTARC009 Unable to read a disk directory block");
      getredrq(drq);
      gspmpfa(req->pagecte);
      req->pcfa.flags = 0;          /* Clearing REQPOT and REQCHECKREAD */
      req->pagecte = NULL;
      req->type = REQNORMALPAGEREAD;
      req->completioncount = 1;     /* Only need one successful read */
      gddenq(req);
      return;
   }
   gspmpfa(cte);
   freedevreqs(req);               /* Return the devreqs */
   getrereq(req);                  /* Return the request */
   if (!(ioinprogress--))         /* Decrement I/O in progress count */
       crash("GRESTARC019 ioinprogress underflow");
   readdiskdirectories(); /* Continue reading */
} /* End diskdirectoryreadended */
 
 
/*********************************************************************
headerextensionreadended - Called after a checkpoint header extension
                           read finishes
 
  Input -
     req     - Pointer to the request that finished
 
  Output - None
*********************************************************************/
static void headerextensionreadended(REQUEST *req)
{
   DEVREQ **drqp = getcompleteddevreqpointer(req);
   struct CkPtHeader *hdr = (struct CkPtHeader *)
                  map_window(IOSYSWINDOW, activecte, MAP_WINDOW_RO);

   if (!drqp)
      crash("GRESTARC007 Unable to read checkpoint header extension");
   hdr = (struct CkPtHeader *)
                  map_window(IOSYSWINDOW, req->pagecte, MAP_WINDOW_RO);
   if (hdr->tod == hdrtod) { /* Ensure from our ckpt */
      DEVREQ *drq = *drqp;         /* No - try other copy */

      consprint("GRESTARC034 Header extension with wrong time stamp\n");
      *drqp = drq->devreq;         /* Dequeue devreq just read */
      if (!req->devreqs)
         crash("GRESTARC008 Unable to read checkpoint header extension");
      getredrq(drq);
      gspmpfa(req->pagecte);
      req->pcfa.flags = 0;          /* Clearing REQPOT and REQCHECKREAD */
      req->pagecte = NULL;
      req->type = REQNORMALPAGEREAD;
      req->completioncount = 1;     /* Only need one successful read */
      gddenq(req);
      return;
   }
   nextheadercte = req->pagecte;   /* Save next header */
   nextheadercte->ctefmt = CheckpointFrame;
   freedevreqs(req);               /* Return the devreqs */
   getrereq(req);                  /* Return the request */
    if (!(ioinprogress--))         /* Decrement I/O in progress count */
       crash("GRESTARC018 ioinprogress underflow");
   readdiskdirectories(); /* Continue reading */
} /* End headerextensionreadended */
 
 
/*********************************************************************
formatdevreq - Add a devreq to a request
 
  Input -
     req     - Pointer to the request to queue devreq from
     sba     - Swap Block Address for devreq
 
  Output -
     zero if no devreqs available, otherwise one
*********************************************************************/
static int formatdevreq(REQUEST *req, uint32 sba)
{
   DEVREQ *drq;
   struct CodeGRTRet sbdl;
 
   if (!sba) return 1;       /* SBA not valid - return ok */
   sbdl = grtfdsba(sba);
   switch (sbdl.code) {
    case grt_notmounted:
      return 1;              /* Not mounted - return ok */
    case grt_mustread:
      drq = acquiredevreq(req);
      if (!drq) return 0;
      drq->device = sbdl.ioret.readinfo.device;
      drq->offset = sbdl.ioret.readinfo.offset;
      md_dskdevreqaddr(drq);
      return 1;
   }
   crash("GRESTARC005 Invalid return code from grtfdsba");
} /* End formatdevreq */
 
 
/*********************************************************************
buildreadrequest - Format a REQUEST to read disk directories
 
  Input -
     sbas    - Pointer to the two swap block addresses to use
     endingproc - Pointer to the procedure to call when I/O finished
 
  Output -
     zero - could not build a request, non-zero request build and queued
*********************************************************************/
static
int buildreadrequest(uint32 *sbas, void (*endingproc)(REQUEST *req))
{
   REQUEST *req = acquirerequest();
//   DEVREQ *drq1, *drq2;
 
   if (!req) return 0;
   req->doneproc = endingproc;
   /* req->pcfa.cda not used */
   req->pcfa.flags = 0;          /* Clearing REQPOT and REQCHECKREAD */
   req->pagecte = NULL;
   req->type = REQNORMALPAGEREAD;
   req->completioncount = 1;     /* Only need one successful read */
   if   (!formatdevreq(req, *sbas)
         || !formatdevreq(req, *(sbas+1))) {
      DEVREQ *drq;
 
      for (drq = req->devreqs; drq; drq = req->devreqs) {
         req->devreqs = drq->devreq;
         getredrq(drq);
      }
      getrereq(req);
      return 0;
   }
   if (!req->devreqs)
      crash("GRESTARC003 Both copies of directory block not mounted");
   ioinprogress++;
   logicalpageio++;
   synciolock = 1;
   gddenq(req);
   synciolock = 0;
   return 1;
} /* End buildreadrequest */
 
 
/*********************************************************************
readdiskdirectories - Read disk directory blocks from the checkpoint
 
  Input - None
 
  Output - None
 
  Notes:
     This routine may be called both from interrupt level and main line
*********************************************************************/
static void readdiskdirectories(void)
{
   if (synciolock) return; /* Limit recursion */
   for (;;) {           /* Process all checkpoint headers */
      struct CkPtHeader *hdr = (struct CkPtHeader *)
                     map_window(IOSYSWINDOW, activecte, MAP_WINDOW_RO);
 
      for (; (headerindex < (hdr->number*2)); headerindex += 2) {
         if (!buildreadrequest(hdr->ddlocs + headerindex,
                               diskdirectoryreadended)) {
            return;
         }
      }
 
         /* Read header extension if any */
 
      if   (hdr->number == headerindex    /* Extension not tested */
            && hdr->extension) {     /* There is an extension */
         if (!buildreadrequest(hdr->extensionlocs,
                               headerextensionreadended))
            return;
         headerindex += 1;   /* Indicate we've queue extension read */
      }
      if (ioinprogress) return;   /* Wait for any I/O */
 
         /* Done processing this header. */
 
      activecte->extensionflags &= ~ctkernellock;  /* Unlock cte */
      gspmpfa(activecte);
      activecte = NULL;
      if (!nextheadercte) {            /* No new header - done */
         gdiswap();                       /* Swap directories */
         readprocessnodepots();           /* Start reading processes */
         return;
      }
      activecte = nextheadercte;       /* Set up to process next hdr */
      nextheadercte = NULL;
      headerindex = 0;
   }
} /* End readdiskdirectories */
 
 
/*********************************************************************
headercomplete - Process when status of checkpoint header is known
 
  Input -
     id      - 1 for the primary header, 2 for secondary, others invalid
     cte     - Pointer to the CTE for the header of NULL
 
  Output - None
*********************************************************************/
static void headercomplete(int id, CTE *cte)
{
   uint64 hrs24;
   static struct CkPtHeader *activeheader;
 
   header[id-1] = cte;                /* Save CTE */
   if (--headerreadsinprogress) return; /* Don't have both - return */
   if (NULL == header[0]) {
      if (NULL == header[1])
         crash("GRESTARC001 Can't read either checkpoint header");
      activecte = header[1];
   } else {
      if (NULL == header[1])
         activecte = header[0];
      else {                     /* Both good - take most recent */
         struct CkPtHeader *hdr0 = (struct CkPtHeader *)
                   map_window(QUICKWINDOW, header[0], MAP_WINDOW_RO);
         struct CkPtHeader *hdr1 = (struct CkPtHeader *)
                   map_window(CKPMIGWINDOW, header[1], MAP_WINDOW_RO);
         if (hdr0->tod < hdr1->tod) { /* hdr0 is older */
            activecte = header[1];
            gspmpfa(header[0]);
         } else {          /* hdr1 is older or both are the same age */
            activecte = header[0];
            gspmpfa(header[1]);
         }
      }
   }
   activeheader = (struct CkPtHeader *)
                  map_window(IOSYSWINDOW, activecte, MAP_WINDOW_RO);
   hdrtod = activeheader->tod;
   if (1 != activeheader->version)
      crash("GRESTARC002 Unrecognized version ID in checkpoint header");
   grtsyncd(activeheader->ddlocs[0], activeheader->ddlocs[1]);
 
      /* Validate the clock. */
 
   hrs24 = 0x0141dd7600000000LL;     /* Set up 24 hours constant */
   grestarttod = delta_calclock2tod(activeheader->calclock);
   if   ((0 == grestarttod)  /* Zero or neg calclock change */
         || (grestarttod > hrs24)) { /* OR > 24 hours */
      consprint("GRESTARC040 Adjusting clock.\n");
      grestarttod = hrs24;         /* Use a 24 hour delta */
   }
   grestarttod += activeheader->tod;
   set_system_timer(grestarttod);
 
   readdiskdirectories();
} /* End headercomplete */
 
 
/*********************************************************************
checkpointheaderreadended - Called when checkpoint header read ends
 
  Input -
     id      - 1 for the primary header, 2 for secondary, others invalid
 
  Output - None
*********************************************************************/
static void checkpointheaderreadended(REQUEST *req)
{
   DEVREQ **drqp = getcompleteddevreqpointer(req);
   CTE *cte = req->pagecte;
 
   if (!drqp) {   /* Nothing finished */
      char buf[80];
      int code = -1;
      DEVREQ *drq = req->devreqs;

      if (drq->status != DEVREQOFFQUEUE) code = drq->status;
      else {
         drq = drq->devreq;
         if (drq && drq->status != DEVREQOFFQUEUE) code = drq->status;
      }
      sprintf(buf, "GRESTARC030 Header %d read failure code=%d\n",
                 (int)req->doneparm, code);
      consprint(buf);
      if (cte) gspmpfa(cte);  /* Free any gotten page */
      headercomplete(req->doneparm, NULL);
   } else {                 /* Read the header */
      struct CkPtHeader *hdr = (struct CkPtHeader *)
                       map_window(QUICKWINDOW, cte, MAP_WINDOW_RO);
      if   (0xf7 != hdr->integritybyte
            || ((hdr->tod >> 32) & 0xffff) != hdr->writecheck) {
         char buf[80];

         sprintf(buf, "GRESTARC031 Header %d has incorrect integrity byte\n",
                 (int)req->doneparm);
         consprint(buf);
         checkpointheaderreread++;  /* Count the re-read */
         gspmpfa(cte);              /* Free the page */
         headercomplete(req->doneparm, NULL);  /* It didn't read */
      } else {
         cte->ctefmt = CheckpointFrame;  /* mark as checkpoint header */
         headercomplete(req->doneparm, cte); /* Success */
      }
   }
   freedevreqs(req);             /* Return the devreqs */
   getrereq(req);                /* Return the request */
} /* End checkpointheaderreadended */
 
 
/*********************************************************************
readheader - Set up request to read the checkpoint header
 
  Input -
     id      - 1 for the primary header, 2 for secondary, others invalid
 
  Output - None
*********************************************************************/
static void readheader(int id)
{
   REQUEST *req = acquirerequest();
   DEVREQ *drq;
   struct CodeGRTRet chl;
 
   if (!req) crash("GRESTARC001 No requests available at boot time");
   req->doneproc = checkpointheaderreadended;
   req->doneparm = id;
   chl = grtchdrl(id);
   switch (chl.code) {
    case grt_notmounted:
      {  char buf[80];
         sprintf(buf, "GRESTARC032 Header %d not mounted\n", id);
         consprint(buf);
      }
      headercomplete(id, NULL);
      return;
    case grt_mustread:
      drq = acquiredevreq(req);
      if (!drq) crash("GRESTARC002 No devreqs available at boot time");
      drq->device = chl.ioret.readinfo.device;
      drq->offset = chl.ioret.readinfo.offset;
      md_dskdevreqaddr(drq);
         /* req->pcfa.cda not used */
      req->pcfa.flags = 0;  /* Clear REQPOT and REQCHECKREAD */
      req->completioncount = 1;
      req->pagecte = NULL;
      req->type = REQNORMALPAGEREAD;
      logicalpageio++;             /* One logical I/O */
      synciolock = 1;          /* Flag for sync finish */
      gddenq(req);                 /* Start the request */
      synciolock = 0;          /* Reset flag */
      return;
   } /* End switch on grtchdrl return code */
   crash("GRESTARC003 Invalid code from grtchdrl");
} /* End readheader */
 
 
/*********************************************************************
grestart - Restart from disk checkpoint
 
  Input -
     proc    - void(void) procedure to call when restarted
 
  Output - None
     Root nodes of all domains with processes in them at the last
     checkpoint have been read in to node frames and placed on the cpu
     queue, the journal page has been updated, and the prime meter
     read and locked into a node frame.
*********************************************************************/
void grestart(void (*proc)(void))
{
   restartendingproc = proc;
   readheader(1);
   readheader(2);
   if (activecte) readdiskdirectories();
   if (moretodo) readprocessnodepots();
} /* End grestart */
