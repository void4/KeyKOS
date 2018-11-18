/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  GCKPTC - Code to take a checkpoint - KeyTech Disk I/O */
 
#include <string.h>
#include <limits.h>
#include "lli.h"
#include "sysdefs.h"
#include "kktypes.h"
#include "kernelpk.h"
#include "timemdh.h"
#include "keyh.h"
#include "checkh.h"
#include "queuesh.h"
#include "wsh.h"
#include "kschedh.h"
#include "ioworkh.h"
#include "ioreqsh.h"
#include "ckpthdrh.h"
#include "dskiomdh.h"
#include "devmdh.h"
#include "spaceh.h"
#include "prepkeyh.h"
#include "getih.h"
#include "gckpth.h"
#include "gdirecth.h"
#include "gmigrath.h"
#include "gswapah.h"
#include "gsw2gckh.h"
#include "grangeth.h"
#include "gdi2gckh.h"
#include "gcleanlh.h"
#include "memoryh.h"
#include "memomdh.h"
#include "locksh.h"
#include "kermap.h" /* for lowcoreflags */
#include "jpageh.h"
#include "consmdh.h"
#include "sparc_mem.h"
#include "memutil.h"
 

/* Local static constants */
 
static const CDA cdaone  = {0, 0, 0, 0, 0, 1};
 
 
/* Local static variables */
 
             /* Phases of checkpoint */
#define P1  0
#define P2  1
#define P3  2
#define P4  3
#define P4b 4
#define P4c 5
#define P5  6
#define P6  7
#define P7  8
#define P8  9
 
static int checkpointphase = P1;         /* Current checkpoint phase */
 
static
CTE *localstackheader = NULL;        /* Pool of page frames */
 
static
CTE *checkpointheader = NULL;        /* Checkpoint header */
static
uint32 checkpointentrycount;         /* Number of disk directory */
                                     /* entries in checkpoint header */
static
uint64 checkpointtod;                   /* Time of checkpoint */
 
#define MAXREASON 4
static
char checkpointreasons[MAXREASON]; /* Reasons for taking this ckpt */
 
uint64 checkpointstarttime;             /* For timing checkpoint */
 
static
uchar ckptflags;
#define PRIMARYHEADERWRITTEN   0x01  /* 1st ckpt header written */
#define SECONDARYHEADERWRITTEN 0x02  /* 2nd ckpt header written */
#define PERMERRORPRIMARY       0x04  /* Perm error writing 1st header */
#define PERMERRORSECONDARY     0x08  /* Perm error writing 2nd header */
#define CHECKPOINTACTIVE       0x10  /* A GCKPTC routine is running */
 
static
struct CalClock checkpointcalclock;  /* Calendar clock at checkpoint */
static
uint16 lastextension;                /* Last extension flag */
static
uint32 lastextensionlocs[2];       /* extension location on disk */
 
static
uint32 checkpointpagecount = 0;
 
 
/* Prototypes for internal routines */
 
static void whencpuvalidprocess(void);
void docheckpoint(void);
 
 
 
 
/*********************************************************************
getlocalframe - Get a local frame from the available list
 
  Input - None
 
  Output -
     Pointer to the Core Table Entry for the local frame or NULL
*********************************************************************/
static CTE *getlocalframe(void)
{
   CTE *cte = localstackheader;
 
   if (cte) localstackheader = cte->hashnext;
   return cte;
} /* End getlocalframe */
 
 
/*********************************************************************
makelocalframeavailable - Put a local frame on the available list
 
  Input -
     cte   - Pointer to the Core Table Entry for the local frame
 
  Output - None
*********************************************************************/
static void makelocalframeavailable(CTE *cte)
{
   cte->hashnext = localstackheader;
   localstackheader = cte;
} /* End makelocalframeavailable */
 
 
/*********************************************************************
marknodes - Mark dirty nodes as needing to be checkpointed
 
  Input - None
 
  Output - None
*********************************************************************/
static void marknodes(void)
{
   NODE *nf;
 
   for (nf = firstnode; nf < anodeend; nf++) {
      if (nf->preplock & 0x80)
         crash("GCKPTC001 Found preplocked node during marknodes");
      if (nf->flags & NFDIRTY && !(nf->flags & NFNEEDSCLEANING)) {
         nf->flags |= NFNEEDSCLEANING;
         nodesmarkedforcleaning++;
      }
   }
} /* End marknodes */
 
 
/*********************************************************************
formatnewheader - Start a new checkpoint header
 
  Input - None
 
  Output - None
*********************************************************************/
static void formatnewheader(void)
{
   struct CkPtHeader *hdr = (struct CkPtHeader *)
            map_window(QUICKWINDOW, checkpointheader, MAP_WINDOW_RW);
 
   hdr->tod = checkpointtod;
   hdr->calclock = checkpointcalclock;
   hdr->number = 0;
   hdr->extension = lastextension;
   hdr->extensionlocs[0] = lastextensionlocs[0];
   hdr->extensionlocs[1] = lastextensionlocs[1];
   hdr->writecheck = (checkpointtod>>32) & 0xffff;
   hdr->version = 1;
   hdr->integritybyte = 0xf7;
   checkpointentrycount = 0;
   checkpointphase = P4;
} /* End formatnewheader */

void unbuildrequest(REQUEST *req)
{
   DEVREQ *drq, *nextdrq;

   for (drq = req->devreqs; drq; drq=nextdrq) {  /* Free the devreqs */
      nextdrq = drq->devreq;
      getredrq(drq);
   }
   req->devreqs = NULL;
   getrereq(req);
}
 
 
/*********************************************************************
buildswapareawriterequest - Build a request to write to the swap area
 
  Input -
     cte        - Pointer to the Core Table Entry for the page to write
     endingproc - Pointer to the procedure to call at end of write
 
  Output -
     Pointer to the request that was built or NULL
*********************************************************************/
static REQUEST *buildswapareawriterequest(CTE *cte,
                                   void (*endingproc)(REQUEST *req))
{
   REQUEST *req = acquirerequest();
   DEVREQ *drq;
   struct Device *(*proc)(int maxstate) = gswfbest;
   DEVICE *dev;
   int built = 0;
 
   if (!req) return NULL;
   req->doneproc = endingproc;
   req->pagecte = cte;
   req->type = REQDIRECTORYWRITE;
   req->completioncount = 2;         /* Directories written twice */
   for (;;) {
      dev = (*proc)(DEVERRORRECOVERY);  /* 1st/next swap device */
      proc = gswnbest;
      if (!dev)
   break;
      drq = acquiredevreq(req);
      if (!drq)
   break;
      drq->device = dev;
      drq->offset = gswppod(dev);
      md_dskdevreqaddr(drq);
      built++;
   }
   if (built < 2) {              /* Couldn't build two - Try later */
crash("couldn't build two");
      unbuildrequest(req);
      return NULL;
   }
   return req;
} /* End buildswapareawriterequest */
 
 
/*********************************************************************
checkpointheaderwriteended - Ending proc for checkpoint header writes
 
  Input -
     req        - Pointer to the request that ended, req->parm is
                  1 for primary header, 2 for secondary
 
  Output - None
*********************************************************************/
static void checkpointheaderwriteended(REQUEST *req)
{
   DEVREQ *drq = req->devreqs;
 
   if (DEVREQCOMPLETE != drq->status) {   /* Write bad */
      if (DEVREQPERMERROR != drq->status) {
         if (DEVREQNODEVICE != drq->status)
            crash("GCKPTC002 Internal error writing checkpoint header");
      } else {
         if (1 == req->doneparm) ckptflags |= PERMERRORPRIMARY;
         else ckptflags |= PERMERRORSECONDARY;
      }
   } else {                /* Write was OK */
      if (1 == req->doneparm) ckptflags |= PRIMARYHEADERWRITTEN;
      else ckptflags |= SECONDARYHEADERWRITTEN;
   }
   getredrq(drq);
   getrereq(req);
   if (!(checkpointpagecount--))
      crash("GCKPTC003 checkpointpagecount underflow");
   if (!(ckptflags & CHECKPOINTACTIVE)) {
      (*continuecheckpoint)();
   }
} /* End checkpointheaderwriteended */
 
 
/*********************************************************************
startcheckpointheaderwrite - Enqueue a checkpoint header write request
 
  Input -
     da         - Info for the write, Code must = grt_mustread
     id         - 1 for primary header, 2 for secondary
 
  Output -
     1 if operation started, otherwise zero
*********************************************************************/
static int startcheckpointheaderwrite(struct CodeGRTRet da, int id)
{
   REQUEST *req = acquirerequest();
   DEVREQ *drq;
 
   if (!req) return 0;
   drq = acquiredevreq(req);
   if (!drq) {
      getrereq(req);
      return 0;
   }
   req->doneproc = checkpointheaderwriteended;
   req->doneparm = id;
   req->pagecte = checkpointheader;
   req->type = REQCHECKPOINTHDRWRITE;
   req->completioncount = 1;
   drq->device = da.ioret.readinfo.device;
   drq->offset = da.ioret.readinfo.offset;
   md_dskdevreqaddr(drq);
   checkpointpagecount++;         /* One more page we're doing */
   logicalpageio++;                      /* One logical I/O */
   gddenq(req);                   /* Do the I/O */
   return 1;
} /* End startcheckpointheaderwrite */
 
 
/*********************************************************************
swapwriteendedcommon - Routine to record the locations of a swap write
 
  Input -
     req    - Pointer to the request that just ended
     locs   - Pointer to 2 uint32s for the Swap Block Addresses
 
  Output - None
*********************************************************************/
static void swapwriteendedcommon(REQUEST *req, uint32 *locs)
{
   DEVREQ *drq, *ndrq;
   int success = 0;          /* Number of successful devreqs */
 
   for (drq = req->devreqs; drq; ) {
      if (success < 2 && DEVREQCOMPLETE == drq->status) {
         locs[success++] = grtslsba(drq->swaploc);
      }
      ndrq = drq->devreq;
      getredrq(drq);
      drq = ndrq;
   }
   if (!success) crash("GCKPTC004 All swap writes failed");
   if (1 == success) locs[1] = 0;   /* Only one loc, 2nd := 0 */
   if (!(checkpointpagecount--))
      crash("GCKPTC005 checkpointpagecount underflow");
   getrereq(req);
   if (!(ckptflags & CHECKPOINTACTIVE)) (*continuecheckpoint)();
} /* End swapwriteendedcommon */
 
 
/*********************************************************************
directorywriteended - Routine called when a directory write finishes
 
  Input -
     req    - Pointer to the request that just ended
 
  Output - None
*********************************************************************/
static void directorywriteended(REQUEST *req)
{
   struct CkPtHeader *ckhdr;
   uint32 *ckhdrdd;
 
   makelocalframeavailable(req->pagecte);  /* Free directory page */
   ckhdr = (struct CkPtHeader *)
           map_window(IOSYSWINDOW, checkpointheader, MAP_WINDOW_RW);
   ckhdrdd = ckhdr->ddlocs + 2*(ckhdr->number++);
   swapwriteendedcommon(req, ckhdrdd);
} /* End directorywriteended */
 
 
/*********************************************************************
extensionwriteended - Routine called when a directory write finishes
 
  Input -
     req    - Pointer to the request that just ended
 
  Output - None
*********************************************************************/
static void extensionwriteended(REQUEST *req)
{
   lastextension = 1;
   swapwriteendedcommon(req, lastextensionlocs);
} /* End extensionwriteended */
 
 
/*********************************************************************
finishcheckpoint - Clean up after checkpoint
 
  Input - None
 
  Output - None
*********************************************************************/
static void finishcheckpoint(void)
{
   CTE *cte;
 
   gspmpfa(checkpointheader);   /* Free checkpoint header */
   checkpointheader = NULL;
   for (cte = localstackheader; cte; cte = localstackheader) {
      localstackheader = cte->hashnext;
      gspmpfa(cte);             /* Free work pages */
   }
   iosystemflags &= ~(INHIBITNODECLEAN | CHECKPOINTMODE);
   continuecheckpoint = NULL;
   checkpointphase = P1;
   iosystemflags |= MIGRATIONINPROGRESS;
   if (lowcoreflags.prtckpt) consprint("migr ");
   enqmvcpu(&migratewaitqueue);
   numberofcheckpointsdone++;
} /* End finishcheckpoint */
 
 
/*********************************************************************
gcktkckp - Start taking a checkpoint
 
  Input -
     reason - Code for reason for taking checkpoint
 
  Output - None
*********************************************************************/
void gcktkckp(unsigned int reason)
{  /* to frustrate checkpoints: return  .....*/;
   if (reason >= MAXREASON)
      crash("GCKPTC006 gcktkckp reason out of range");
   if (!(iosystemflags & CHECKPOINTMODE)) {
      checkpointreasons[reason] = 1;
      if (iosystemflags & MIGRATIONINPROGRESS) {
         if (!(iosystemflags & CHECKPOINTATENDOFMIGRATION)) {
            iosystemflags |= CHECKPOINTATENDOFMIGRATION;
            gmiimpr();
         }
         if (TKCKPCKPTAFTERMIGR == reason)
            crash("GCKPTC007 Ckpt after migration during migration");
         if   (TKCKPKEYCALL != reason
               && !(iosystemflags & MIGRATIONURGENT)) {
            /* Must stop all domains but external migrator */
            /*  and wait for the migration to complete */
            iosystemflags |= MIGRATIONURGENT;
            runmigr();
         }
      } else {        /* Neither checkpoint nor migration in progress */
         iosystemflags |= CHECKPOINTMODE;  /* Checkpointing now */
         if (lowcoreflags.prtckpt) consprint("ckpt ");
         if (TKCKPCKPTAFTERMIGR == reason) {
            gmidmpr();              /* No longer need priority migr */
            if (iosystemflags & MIGRATIONURGENT) {
               iosystemflags &= ~MIGRATIONURGENT; /* No longer urgent */
               slowmigr();         /* Run domains besides migrator */
            }
         }
         stopdisp();
         checkpointstarttime = read_system_timer();
         waitstateprocess = whencpuvalidprocess;
 
         /* One checkpoint may be taken for more than one reason */
         if (checkpointreasons[0]) checkpointfordirentries++;
         if (checkpointreasons[1]) checkpointforswapspace++;
         if (checkpointreasons[2]) checkpointkeycall++;
         if (checkpointreasons[3]) checkpointduringmigration++;
         memzero(checkpointreasons, sizeof checkpointreasons);
         numberofcheckpointsstarted++;
      }
   } else if (TKCKPCKPTAFTERMIGR == reason)
      crash("GCKPTC008 Ckpt after migration during migration");
} /* End gcktkckp */
 
 
/*********************************************************************
whencpuvalidprocess - Called when world is valid and no domains are
                      to be run
 
  Input - None
 
  Output - None
*********************************************************************/
static void whencpuvalidprocess(void)
{
   CTE *cte;
 
   check();                  /* Don't checkpoint invalid state */
 
      /* Reset flags */
   ckptflags &= ~(PRIMARYHEADERWRITTEN | SECONDARYHEADERWRITTEN
                  | PERMERRORPRIMARY | PERMERRORSECONDARY);
      /* Scan all pages for ones that need to be written */
#if LATER
/* jcv - function was removed from sparc_mem.c and previously did
 * nothing. We may want to remove this call.
 */
   updatechangedbits();        /* Ensure bits are updated */
#endif
   for (cte = firstcte; cte < lastcte; cte++) {
      /* Journal page is always locked */
      if ((cte->corelock > HILRU) && Memcmp(cte->use.page.cda, "\0\0\0\0\0\1", 6))
         crash("GCKPTC009 Page locked in whencpuvalidprocess");
      if (cte->iocount || cte->flags & ctchanged) {
         switch (cte->ctefmt) {
          case AlocPotFrame:
            crash("GCKPTC010 Dirty allocation pot");
            break;
          case PageFrame:
            if (cte->busaddress >= endmemory  /* an I/O page */
                && cte->use.page.dontcheckpoint)
               break;   /* don't checkpoint this I/O page */
            /* Else fall into NodePot case */
          case NodePotFrame:
            if (!(cte->flags & ctkernelreadonly)) gckincpc(cte);
            break;
          default:               /* Ignore all other types */
            break;
         }
      }
   }
   checkpointtod = read_system_timer();
   checkpointcalclock = read_calendar_clock();
   lastextension = 0;
   waitstateprocess = nowaitstateprocess;
   continuecheckpoint = docheckpoint;
   checkpointphase = P2;
   checkforcleanstart();
} /* End whencpuvalidprocess */
 
 
/*********************************************************************
checkpointphase2 - Called to start or continue the node cleaning process
 
  Input - None
 
  Output -
     Returns one if next checkpoint phase should run, zero to quit for
     the moment.
*********************************************************************/
static int checkpointphase2(void)
{
   for (;;) {
      for ( ; gspcleannodes(); ) ;
      if (nodesmarkedforcleaning) {
         if (!gddstartpageclean())
   break;
      } else {                 /* No nodes marked for cleaning */
         marknodes();
         if (!nodesmarkedforcleaning) { /* All nodes cleaned */
            int i;               /* Loop counter */
            uint64 time;         /* For calculating phase 2 time */
 
            iosystemflags |= INHIBITNODECLEAN; /* Don't clean nodes */
            gclfreze(1);         /* Don't add to the clean list */
            checkpointheader = gspgpage();
            if (!checkpointheader)
   break;
            checkpointheader->ctefmt = CheckpointFrame;
            clear_page(checkpointheader);
            i = gswnumsd();
            if (i <= 0) i = 1;
            for ( ; i--; ) {            /* Build pool of pages for */
               CTE *ddp = gspgpage();   /* disk directories */
               if (!ddp)
            break;
               ddp->ctefmt = DiskDirFrame;
               makelocalframeavailable(ddp);
            }
            if (!localstackheader) {      /* No disk dir pages */
               gspmpfa(checkpointheader);
               checkpointheader = NULL;
   break;
            }
 
            /* Start dispatching domains again */
 
            iosystemflags &=~DISPATCHINGDOMAINSINHIBITED;
            enqmvcpu(&frozencpuqueue);
            time = read_system_timer();   /* Calculate ckpt outage */
            //llisub(&time, &checkpointstarttime);
            time -= checkpointstarttime;
            //if (llicmp(&maxckptp2time, &time) < 0) maxckptp2time = time;
            if(*(uint64*)&maxckptp2time < time) *(uint64*)&maxckptp2time = time;
            *(uint64*)&currentckptp2time = time;
/*{char str[80];
 sprintf(str,"end phase 2, pagecount=%x\n",checkpointpagecount);
 consprint(str);}*/
            checkpointphase = P3;    /* Go on to phase 3 */
            return 1;
         }
      }
   }
   return 0;
} /* End checkpointphase2 */
 
 
/*********************************************************************
checkpointphase3 - Wait for nodepot writes to swap area to finish
 
  Input - None
 
  Output -
     Returns one if next checkpoint phase should run, zero to quit for
     the moment.
*********************************************************************/
static int checkpointphase3(void)
{
   for (;;) {               /* Kick page clean until all done */
      if (checkpointpagecount) {  /* Still pages to clean */
         if (!gddstartpageclean())
   break;                           /* Can't clean any more */
      } else {                    /* All nodepots written */
         gdintsdr();        /* Start building disk directory pages */
         formatnewheader(); /* Format a new checkpoint header */
         return 1;          /* Phase 4 set in formatnewheader */
      }
   }
   return 0;
} /* End checkpointphase3 */
 
 
/*********************************************************************
checkpointphase4 - Build disk directory blocks and write them
 
  Input - None
 
  Output -
     Returns one if next checkpoint phase should run, zero to quit for
     the moment.
*********************************************************************/
static int checkpointphase4(void)
{
   ckptflags |= CHECKPOINTACTIVE;
   for (;;) {
      CTE *lcte = getlocalframe();
      uchar *p;
      REQUEST *req;
 
      if (!lcte)
   break;
      req = buildswapareawriterequest(lcte, directorywriteended);
      if (!req) {
         makelocalframeavailable(lcte);
   break;
      }
      p = map_window(IOSYSWINDOW, lcte, MAP_WINDOW_RW);
      if (!gdindp(p, checkpointtod)) {
         unbuildrequest(req);
         makelocalframeavailable(lcte);
         checkpointphase = P5;
         return 1;
      }
      lcte->flags &= ~ctreferenced;  /* Reset the referenced flag */
      checkpointpagecount++;         /* One more page we're doing */
      logicalpageio++;                      /* One logical I/O */
      gddenq(req);                   /* Do the I/O */
      checkpointentrycount++;        /* Plus 1 dd entry in header */
      if (checkpointentrycount >= CKPTHEADERNUMDD) { /* Header full */
         checkpointphase = P4b;
         return 1;
      }
   }
   ckptflags &= ~CHECKPOINTACTIVE;   /* No longer active */
   return 0;
} /* End checkpointphase4 */
 
 
/*********************************************************************
checkpointphase4b - Wait until directories are written, then write the
                    checkpoint header extension
 
  Input - None
 
  Output -
     Returns one if next checkpoint phase should run, zero to quit for
     the moment.
*********************************************************************/
static int checkpointphase4b(void)
{
   REQUEST *req;
 
   if (checkpointpagecount) return 0;
   ckptflags |= CHECKPOINTACTIVE;
   req = buildswapareawriterequest(checkpointheader,
                                   extensionwriteended);
   if (req) {                 /* Request built */
      checkpointheader->flags &= ~ctreferenced;
      checkpointpagecount++;
      logicalpageio++;                      /* One logical I/O */
      gddenq(req);                   /* Do the I/O */
      checkpointphase = P4c;
      return 1;
   }
   return 0;
} /* End checkpointphase4b */
 
 
/*********************************************************************
checkpointphase4c - Wait until checkpoint header extension is written,
                    then start new header
 
  Input - None
 
  Output -
     Returns one if next checkpoint phase should run, zero to quit for
     the moment.
*********************************************************************/
static int checkpointphase4c(void)
{
   if (checkpointpagecount) return 0;
   ckptflags |= CHECKPOINTACTIVE;    /* Checkpoint is active */
   formatnewheader();              /* Format a new checkpoint header */
   return 1;                       /* Phase 4 set in formatnewheader */
} /* End checkpointphase4c */
 
 
/*********************************************************************
checkpointphase5 - Wait until directories are written and then write
                   first main checkpoint header
 
  Input - None
 
  Output -
     Returns one if next checkpoint phase should run, zero to quit for
     the moment.
*********************************************************************/
static int checkpointphase5(void)
{
   struct CodeGRTRet da;
 
   if (checkpointpagecount) return 0;
   ckptflags |= CHECKPOINTACTIVE;    /* Checkpoint is active */
   da = grtchdrl(1);               /* 1st checkpoint header location */
   if (grt_mustread == da.code) {    /* First is mounted */
      if (startcheckpointheaderwrite(da, 1)) {
         checkpointphase = P6;
         return 1;
      }
   } else {
      checkpointphase = P6;
      return 1;
   }
   return 0;
} /* End checkpointphase5 */
 
 
/*********************************************************************
checkpointphase6 - Wait until first header is written and then write
                   second main checkpoint header
 
  Input - None
 
  Output -
     Returns one if next checkpoint phase should run, zero to quit for
     the moment.
*********************************************************************/
static int checkpointphase6(void)
{
   struct CodeGRTRet da;
 
   if (checkpointpagecount) return 0;
   ckptflags |= CHECKPOINTACTIVE;    /* Checkpoint is active */
   da = grtchdrl(2);               /* 2nd checkpoint header location */
   if (grt_mustread == da.code) {    /* First is mounted */
      if (startcheckpointheaderwrite(da, 2)) {
         checkpointphase = P7;
         return 1;
      }
   } else {
      if (ckptflags & PRIMARYHEADERWRITTEN) {  /* Wrote one */
         checkpointphase = P7;
         return 1;
      } else {               /* None written - try again after mount */
         consprint("GCK445 Can't write checkpoint header");
         checkpointphase = P5;
         return 0;
      }
   }
   return 0;
} /* End checkpointphase6 */
 
 
/*********************************************************************
checkpointphase7 - Wait until second header is written and then
                   complete the checkpoint
 
  Input - None
 
  Output -
     Returns one if next checkpoint phase should run, zero to quit for
     the moment.
*********************************************************************/
static int checkpointphase7(void)
{
   if (checkpointpagecount) return 0; /* Wait for writes to complete */
   ckptflags |= CHECKPOINTACTIVE;    /* Checkpoint is active */
   if (!(ckptflags & (PRIMARYHEADERWRITTEN | SECONDARYHEADERWRITTEN))) {
      if ((PERMERRORPRIMARY | PERMERRORSECONDARY) ==
           ckptflags & (PERMERRORPRIMARY | PERMERRORSECONDARY))
         crash("GCKPTC011 Perm error writting both checkpoint headers");
      consprint("GCK446 Can't write checkpoint header");
      checkpointphase = P5;           /* Wait for disk mount */
      return 0;
   } else {
      gclfreze(0);      /* Unfreeze the clean list */
      gdiswap();        /* Swap swap area directories */
      checkpointphase = P8;
      return 1;
   }
} /* End checkpointphase7 */
 
 
/*********************************************************************
checkpointphase8 - Read journal page and update time of checkpoint
 
  Input - None
 
  Output -
     Returns one if next checkpoint phase should run, zero to quit for
     the moment.
*********************************************************************/
static int checkpointphase8(void)
{
   CTE *cte = srchpage(cdaone);
   struct KernelPage *kp;
 
   if (!cte) {
      struct CodeIOret ret;
 
      ret = getreqp(cdaone, REQNORMALPAGEREAD, getended, NULL);
      switch (ret.code) {
       case io_notmounted:
         finishcheckpoint();      /* Don't update tod */
         return 0;
       case io_notreadable:
         finishcheckpoint();      /* Don't update tod */
         return 0;
       case io_pagezero:       /* ioret *PCFA for virtual zero page */
         cte = getlocalframe();
         cte->ctefmt = FreeFrame; /* like we just got it */
         setupvirtualzeropage(ret.ioret.pcfa, cte); /* set it up */
      break;
       case io_started:
         /* Fall thru into not started cases */
       case io_cdalocked:      /* CDA may already be in transit */
       case io_noioreqblocks:
         return 0;               /* And wait for it to come in */
       default: crash("GCKPTC012 getreqp returned bad code");
         finishcheckpoint();      /* Don't update tod */
         return 0;
      }
   }
   /* cte has core table entry of kernel page */
   kp = (struct KernelPage *)
                       map_window(QUICKWINDOW, cte, MAP_WINDOW_RW);
   /* kp points to the kernel page */
   kp->KP_LastCheckPointTOD = checkpointtod;
   finishcheckpoint();
   return 0;
} /* End checkpointphase8 */
 
 
/*********************************************************************
docheckpoint - Perform the steps necessary for a checkpoint
 
  Input - None
 
  Output - None
*********************************************************************/
void docheckpoint(void)
{
   int cont = 1;
 
   for (;cont;) {
      switch (checkpointphase) {
       case P2:
         cont = checkpointphase2();
         break;
       case P3:
         cont = checkpointphase3();
         break;
       case P4:
         cont = checkpointphase4();
         break;
       case P4b:
         cont = checkpointphase4b();
         break;
       case P4c:
         cont = checkpointphase4c();
         break;
       case P5:
         cont = checkpointphase5();
         break;
       case P6:
         cont = checkpointphase6();
         break;
       case P7:
         cont = checkpointphase7();
         break;
       case P8:
         cont = checkpointphase8();
         break;
       default:
         crash("GCKPTC022 Invalid checkpoint phase");
      }
   }
   ckptflags &= ~CHECKPOINTACTIVE;   /* No longer active */
} /* End docheckpoint */
 
 
/*********************************************************************
gckincpc - Increment Checkpoint Page Count and make page Kernel R/O
 
  Input -
     cte      - Pointer to the Core Table Entry for the page
 
  Output - None
*********************************************************************/
void gckincpc(CTE *cte)
{
   checkpointpagecount++;
   makekro(cte);
} /* End gckincpc */
 
 
/*********************************************************************
gckdecpc - Decrement Checkpoint Page Count and reset page Kernel R/O
 
  Input -
     cte      - Pointer to the Core Table Entry for the page
 
  Output - None
*********************************************************************/
void gckdecpc(CTE *cte)
{
   if (0 == checkpointpagecount--)
      crash("GCKPTC013 checkpointpagecount went negative");
   resetkro(cte);
} /* End gckdecpc */
