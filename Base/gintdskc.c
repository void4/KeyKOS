/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* gintdskc.c - I/O INTERRUPT ROUTINES FOR GNOSIS DISK I/O */
#include <stddef.h> /* offsetof is here */
#include <string.h>
#include "sysdefs.h"
#include "ioreqsh.h"
#include "gcoh.h"
#include "dskiomdh.h"
#include "spaceh.h"
#include "ktqmgrh.h"
#include "mi2mdioh.h"
#include "getih.h"
#include "gbadpagh.h"
#include "gdskdvrh.h"
#include "ioworkh.h"
#include "wsh.h"
#include "alocpoth.h"
#include "disknodh.h"
#include "kermap.h"
#include "queuesh.h"
#include "gckpth.h"
#include "memomdh.h"
#include "gcleanlh.h"
#include "devmdh.h"
#include "scafoldh.h"
#include <stdio.h>
#include "memutil.h"


void disk_kertask_function(
   struct KernelTask *ktp)
{
   PHYSDEV *pdev;
   /* Calculate backwards from ktp to get pdev. */
   pdev = (PHYSDEV *)(  (char *)ktp
                      - offsetof(PHYSDEV, kertask));
   pdev->doneproc(pdev);
}

void diskdone_intproc(
   struct scsi_pkt *sp)
/* Runs at interrupt level. */
{
   PHYSDEV *pdev;
   /* Calculate backwards from scp to get pdev. */
   pdev = (PHYSDEV *)(sp->pkt_private);
   if (sp->pkt_reason != 0/*CMD_CMPLT*/
       || !(sp->pkt_state & 16/*STATE_GOT_STATUS*/)
       || sp->pkt_resid != 0 /* some residual count */){
      pdev->donestatus = PACKDISMOUNTED;
   }else if (!(((struct scsi_status *)sp->pkt_scbp)->sts_chk)
       && !(((struct scsi_status *)sp->pkt_scbp)->sts_busy)){
      pdev->donestatus = NOERROR;
   } else {
      crash("GDD001 Disk error");
   }
   pdev->diskerrorretrycounter = 0;
// pdev->lastaddress = GETG0ADDR((union scsi_cdb *)sp->pkt_cdbp);
   {union scsi_cdb * y = ((union scsi_cdb *)sp->pkt_cdbp);
     int x = ((y->cdb_un.tag & 0x1F) << 16) + (y->cdb_un.sg.g0.addr1 << 8)
        +(y->cdb_un.sg.g0.addr0);
     pdev->lastaddress = x;}
   enqueue_kernel_task(&(pdev->kertask));
}

static void updatedeviceservicetime(
   PHYSDEV *pdev)
{}

void free_any_page(
   REQUEST *req)
{
   if (req->pcfa.flags & REQPAGEALLOCATED) {
      gspmpfa(req->pagecte);
      req->pagecte = NULL;
      req->pcfa.flags &= ~REQPAGEALLOCATED;
   }
}

static void markpageioerror(
   DEVREQ *drq)
/* Process a DEVREQ with permanent I/O error. */
{
   REQUEST *req = drq->request;

   gbadlog(drq->device, drq->offset);
   drq->status = DEVREQPERMERROR;
   free_any_page(req);
   req->completioncount++;
   check_request_done(req);
}

static void countendingsbytype(
   int type)
/* Bump #IOINDED[type] and #DISKIO[type] */
{}

void enddevreq(DEVREQ *drq)
{
   REQUEST *req = drq->request;
   PHYSDEV *pdev = drq->device->physdev;
   unsigned char *pageaddr;

if(lowcoreflags.iologenable){char str[80];
 sprintf(str,"enddevreq %x\n", (int)drq);
 logstr(str);}
   countendingsbytype(req->type);
   switch (req->type) {
    case REQNORMALPAGEREAD:
    case REQMIGRATEREAD:
    case REQRESYNCREAD:
      /* End of a read. */
      pageaddr = window_address(pdev->windownum);
      /* Perform integrity checks. */
      /* Check that the whole page was read. */
      if (drq->flags & DEVREQSECONDTRY) {
         if (Memcmp(pageaddr+4092, "wxyz", 4) == 0)
            crash("Whole page didn't read");
      } else { /* first try */
         if (Memcmp(pageaddr+4092, "abcd", 4) == 0) {
            drq->flags |= DEVREQSECONDTRY;
            free_any_page(req);
            gcodidnt(drq);
            return;
         }
      }
      if (req->pcfa.flags & REQCHECKREAD) {
         if (Memcmp(pageaddr, pageaddr+4092, 4) != 0) {
            readerrors++; /* count the error */
            markpageioerror(drq);
            return;
         }
      }
      if (req->pcfa.flags & REQPOT) {
         if (req->pcfa.flags & REQALLOCATIONPOT) {
            /* An allocation pot. */
#define apot ((struct AlocPot *)pageaddr)
            if (apot->checkbyte
                != (apot->entry[0].flags & ADATAINTEGRITY
                    | ~*(((unsigned char *)&apot->checkbyte)-1)
                      & 0xff^ADATAINTEGRITY ) ) { /* error */
               readerrors++; /* Count the error */
               markpageioerror(drq);
               return;
            }
         } else {
            /* a node pot */
            if (req->potaddress.range >= 0) {
               /* a home node pot */
#define npot ((struct NodePot *)pageaddr)
               if (npot->checkbyte
                   != ((npot->disknodes[0].flags & DNINTEGRITY)
                       | ~*(((unsigned char *)&npot->checkbyte)-1)
                         & 0xff^DNINTEGRITY ) ) { /* error */
                  readerrors++; /* Count the error */
                  markpageioerror(drq);
                  return;
               }
            }
         }
      } /* end of if a pot */
      req->pagecte->flags &= ~ctchanged; /* Reading it in doesn't
              constitute changing it. */
      break;
    default: break;
      /* If a write, no checks to perform. */
   }
   /* DEVREQSTATUS here is either DEVREQSELECTED or DEVREQABORTOREND */
   drq->status = DEVREQCOMPLETE;
   check_request_done(req);
} /* end of enddevreq */

void ginioida(PHYSDEV *pdev)
/* Handle end of I/O operation (workunit has a DEVREQ *) */
{
   if (pdev->enqstate != DEVRUNNINGQUEUE)
      crash("GIN504 device not running");
   switch (pdev->donestatus) {
    case PERMERROR:
      markpageioerror((DEVREQ *)pdev->workunit);
      restart_disk(pdev);
      break;
    case PACKDISMOUNTED:
      pdev->enqstate = DEVNOTREADY;
      {  DEVREQ *drq = (DEVREQ *)pdev->workunit;
         REQUEST *req;
         drq->status = DEVREQNODEVICE;
         req = drq->request;
         free_any_page(req);
         req->completioncount++;
         check_request_done(req);
      }
      gcodismtphys(pdev);
      break;
    case NOERROR:
      updatedeviceservicetime(pdev);
      enddevreq((DEVREQ *)pdev->workunit);
      restart_disk(pdev);
      break;
   }
   return_completed_requests();
   gcoreenq();
   checkforcleanstart();
} /* end of ginioida */

void reclean_pages(
   PHYSDEV *pdev)
/* Called after failure to clean a page. */
{
   CTE *cte = (CTE *)pdev->workunit;
   if (cte->iocount-- == 0) crash("GIN005 iocount underflow");
   cte->flags |= ctchanged; /* It wasn't cleaned */
   if (PageFrame == cte->ctefmt)
      mark_page_unreferenced(cte);
   else cte->flags &= ~ctreferenced; /* Kernel's access doesn't count */
   gdinperr(cte);
   gcladd(cte);
   recleans++; /* Keep statistics */
}

void ginioicp(PHYSDEV *pdev)
/* Handle end of I/O operation to clean a page.
   workunit has a CTE *. */
{
   if (pdev->enqstate != DEVRUNNINGQUEUE)
      crash("GIN504 device not running");
   switch (pdev->donestatus) {
    case PERMERROR:
      reclean_pages(pdev);
      restart_disk(pdev);
      break;
    case PACKDISMOUNTED:
      reclean_pages(pdev);
      gcodismtphys(pdev);
      break;
    case NOERROR:
      updatedeviceservicetime(pdev);
      {  CTE *cte = (CTE *)pdev->workunit;

         if (iosystemflags & CHECKPOINTMODE)
            if (iosystemflags & DISPATCHINGDOMAINSINHIBITED)
               cleansckpt2++;
            else cleansckptx++;
         else cleansnockpt++;
         gdiset(cte, pdev->ccwswaploc); /* Update/create dir entry */
         if (cte->iocount-- == 0) crash("GIN003 iocount underflow");
         if (cte->iocount == 0) { /* Went to zero */
if(lowcoreflags.iologenable){char str[80];
 sprintf(str,"ginioicp cte=%x ", (int)cte);
 logstr(str);}
	    /* Journal page is always locked. */
            if ((cte->corelock & 0xf8) && Memcmp(cte->use.page.cda, "\0\0\0\0\0\1", 6))
	       crash("GIN004 page locked");
            if (cte->flags & ctkernelreadonly)
               /* Page was kernelreadonly. It must still be clean. */
               gckdecpc(cte);
            if (cte->ctefmt == NodePotFrame) {
               /* A node pot. It must still be clean. */
               getqnodp(cte); /* put on nodepot hash chain */
               cte->flags &= ~ctreferenced; /* Kernel reference doesn't count */
            }
            else mark_page_unreferenced(cte);
/* If some user referenced the page while it was being cleaned,
   it won't be noticed. Too bad. */
            if (iosystemflags & WAITFORCLEAN) { /* Shake things loose */
               serve_nopagesrequestqueue();
               enqmvcpu(&nopagesqueue);
            }
         }
      }
      restart_disk(pdev);
      break;
   }
   return_completed_requests();
   gcoreenq();
   checkforcleanstart();
} /* end of ginioicp */

