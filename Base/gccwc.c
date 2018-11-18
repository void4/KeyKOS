/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* gccwc.c - Build Disk I/O commands */
#include "sysdefs.h"
#include "keyh.h" /* for pagesize */
#include "ioreqsh.h"
#include "dskiomdh.h"
#include "gccwh.h"
#include "gswapah.h"
#include "memomdh.h"
#include "spaceh.h"
#include "memutil.h"
#include <string.h>

extern struct scsi_pkt * esp_scsi_init_pkt();

unsigned int iobuild[8];
unsigned int iocbuild;

static enum ccwsret buildpageread(DEVREQ *drq)
/* If CCWSBUILT returned, physdev->enqstate is set to DEVRUNNINGQUEUE. */
/* If NOPAGES returned, caller must call restart_disk(drq->device->physdev)
   (because we may have temporarily locked out cleaning). */
/* Caller must call checkforcleanstart. */
{
   PHYSDEV *pdev = drq->device->physdev;
   REQUEST *req = drq->request;
   CTE *cte;
   unsigned char *pageaddr; /* kernel virtual address of page */
   struct buf *bufp;

   if (pdev->enqstate != DEVSTART) crash("GCC541 disk busy");
   pdev->enqstate = DEVRUNNINGQUEUE;  /* Reserve the device
         so it won't be used for cleaning */
   cte = req->pagecte;
   if (cte == NULL) { /* must allocate a page */
      if (!(cte = gspgpage())) {
         /* Caller must release the disk (and check if
                cleaning became necessary) */
         /* Don't call restart_disk here because that leads to
            unbounded recursion. */
         return NOPAGES;
      }
      req->pagecte = cte;
      req->pcfa.flags |= REQPAGEALLOCATED;
      cte->ctefmt = InTransitFrame;
   }
   cte->extensionflags |= ctkernellock;
   {
      pageaddr = map_any_window(pdev->windownum, cte->busaddress, MAP_WINDOW_RW);
   }
   /* Set read check pattern at end of the page. */
   Memcpy(pageaddr+4092,
          (drq->flags & DEVREQSECONDTRY ? "wxyz" : "abcd"),
          4);
   /* Build scsi_cmd. */
   bufp = &pdev->pd_scsi_buf;
   bufp->b_un.b_addr = (caddr_t)pageaddr;
   bufp->b_flags = B_READ;
   bufp->b_bcount = pagesize;
   pdev->pd_dynamic_sp = esp_scsi_init_pkt (&pdev->pd_scsi_addr,
       (struct scsi_pkt *) NULL,
        &pdev->pd_scsi_buf, 6/*CDB_GROUP0*/, 1, 8, PKT_CONSISTENT);
   {  unsigned long diskaddr = drq->device->physoffset
             + drq->offset*BLOCKSPERPAGE; /* block address */
      makecmd_g1(pdev->pd_dynamic_sp, 40/*SCMD_READ_G1*/, diskaddr, BLOCKSPERPAGE);
   }

   pdev->workunit = drq;
   return CCWSBUILT;
} /* End buildpageread */

static void buildwritetopageondevice(
   CTE *cte,
   PHYSDEV *pdev,
   unsigned long diskaddr)  /* Block address on pdev */
/* pdev->enqstate is set to DEVRUNNINGQUEUE. */
{
   unsigned char *pageaddr; /* kernel virtual address of page */
   struct buf *bufp;

   if (pdev->enqstate != DEVSTART) crash("GCC003 Device in use");
   pdev->enqstate = DEVRUNNINGQUEUE;  /* Reserve the device */
   {  
      pageaddr = map_any_window(pdev->windownum, cte->busaddress, MAP_WINDOW_RO);
   }
   /* Build scsi_cmd. */
   bufp = &pdev->pd_scsi_buf;
   bufp->b_un.b_addr = (caddr_t)pageaddr;
   bufp->b_flags = B_WRITE;
   bufp->b_bcount = pagesize;
   pdev->pd_dynamic_sp = esp_scsi_init_pkt (&pdev->pd_scsi_addr,
       (struct scsi_pkt *) NULL,
        &pdev->pd_scsi_buf, 6/*CDB_GROUP0*/, 1, 8, PKT_CONSISTENT);
   makecmd_g1(pdev->pd_dynamic_sp, 42/*SCMD_WRITE_G1*/, diskaddr, BLOCKSPERPAGE);
   return;
} /* End buildwritetopageondevice */

static enum ccwsret buildpagewrite(DEVREQ *drq)
/* If CCWSBUILT returned, physdev->enqstate is set to DEVRUNNINGQUEUE. */
{
   REQUEST *req = drq->request;

   buildwritetopageondevice(req->pagecte,
                            drq->device->physdev,
                            drq->offset*BLOCKSPERPAGE
                            + drq->device->physoffset );
   drq->device->physdev->workunit = drq;
   return CCWSBUILT;
} /* End buildpagewrite */

static enum ccwsret builddirectorywrite(DEVREQ *drq)
/* If CCWSBUILT returned, physdev->enqstate is set to DEVRUNNINGQUEUE. */
{
   REQUEST *req = drq->request;
   LOGDISK *ldev = drq->device;
   struct CodeGRTRet cgrtr;

   cgrtr = gswnext(ldev);
   if (cgrtr.code != grt_mustread)  /* no swap space on device */
      return NOSWAPSPACE;
   if (cgrtr.ioret.readinfo.device != ldev)
      crash("GCC005 gswnext changed device");
   drq->swaploc = cgrtr.ioret.readinfo.id.potaddress;
         /* Return swaploc for requestor. */
   buildwritetopageondevice(req->pagecte,
                            ldev->physdev,
                            cgrtr.ioret.readinfo.offset*BLOCKSPERPAGE
                            + ldev->physoffset);
   drq->device->physdev->workunit = drq;
   return CCWSBUILT;
} /* End builddirectorywrite */
 
void gccicln(void)         /* New group of frames to clean */
{
}
 
int gccbncs(
/* Add page to group of pages being cleaned. */
/* Returns 0 if could not add
      (because no swap space, or group is full). */
   LOGDISK *ldev,
   CTE *cte)
/* If 1 returned, physdev->enqstate is set to DEVRUNNINGQUEUE. */
{
   struct CodeGRTRet cgrtr;

   if (ldev->physdev->enqstate != DEVSTART)
      return 0; /* Must have already enqueued one.
                   (We only handle groups of one!) */
   cgrtr = gswnext(ldev);
   if (cgrtr.code != grt_mustread) { /* no swap space on device */
      return 0; /* NOSWAPSPACE */
   }
   if (cgrtr.ioret.readinfo.device != ldev)
      crash("GCC006 gswnext changed device");
   if (!cte->iocount) crash("GCC833 cleaning, iocount zero");
   ldev->physdev->ccwswaploc = cgrtr.ioret.readinfo.id.potaddress;
         /* Save the swaploc. */
   buildwritetopageondevice(cte,
                            ldev->physdev,
                            cgrtr.ioret.readinfo.offset*BLOCKSPERPAGE
                            + ldev->physoffset);
   ldev->physdev->workunit = cte;
   iocbuild++; /* Count number of cleans */
   return 1;   /* CCWSBUILT */
} /* End gccbncs */

enum ccwsret build_ccws(
   DEVREQ *drq)  /* The devreq is off the device queue and marked
            DEVREQSELECTED */
/* drq->device->physdev->enqstate is DEVSTART */
/* Caller must call checkforcleanstart. */
/* If NOPAGES returned, caller must call restart_disk(drq->device->physdev). */
{
   REQUEST *req = drq->request;
   enum ccwsret retval;

   switch (req->type) {
    case REQNORMALPAGEREAD:
      retval = buildpageread(drq);
      break;
    case REQDIRECTORYWRITE:
      retval = builddirectorywrite(drq);
      break;
    case REQCHECKPOINTHDRWRITE:
      retval = buildpagewrite(drq);
      break;
    case REQMIGRATEREAD:
      retval = buildpageread(drq);
      break;
    case REQMIGRATEWRITE:
      retval = buildpagewrite(drq);
      break;
    case REQJOURNALIZEWRITE:
      crash("Journal write not implemented...");
      break;
    case REQRESYNCREAD:
      retval = buildpageread(drq);
      break;
    case REQHOMENODEPOTWRITE:
      retval = buildpagewrite(drq);
      break;
    default: crash("GCC000 Unknown request type");
   }
   if (retval == CCWSBUILT) {
      /* Count number of builds */
      iobuild[(int)req->type]++;
   }
   return retval;
} /* end of build_ccws */
