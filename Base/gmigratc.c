/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  GMIGRATEC - Migrate swap area to home - KeyTech Disk I/O */
 
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include "lli.h"
#include "sysdefs.h"
#include "kerinith.h"
#include "migrate.h"
#include "keyh.h"
#include "queuesh.h"
#include "locksh.h"
#include "memomdh.h"
#include "disknodh.h"
#include "cpujumph.h"
#include "cpumemh.h"
#include "wsh.h"
#include "prepkeyh.h"
#include "ioreqsh.h"
#include "ioworkh.h"
#include "dskiomdh.h"
#include "devmdh.h"
#include "spaceh.h"
#include "gbadpagh.h"
#include "getih.h"
#include "get2gmih.h"
#include "gckpth.h"
#include "gdirecth.h"
#include "gdi2gmih.h"
#include "gmigrath.h"
#include "gmi2gclh.h"
#include "gswapah.h"
#include "grangeth.h"
#include "grt2gmih.h"
#include "gupdpdrh.h"
#include "cvt.h"
#include "kermap.h" /* for lowcoreflags */
#include "getmntfh.h"
#include "consmdh.h"
#include "locore.h" 
#include "timemdh.h" 
#include "memutil.h"

 
/* Local static constants */
 
static const CDA cdaone = {0, 0, 0, 0, 0, 1};
 
/* Normal priority table indexed by type of request */
 
static const int normalprioritytable[] =
              {2,     /* Normal Page Read */
               3,     /* Directory Write */
               1,     /* Checkpoint Header Write */
               5,     /* Migrate Read */
               4,     /* Migrate Write */
               2,     /* Journal Write */
               6,     /* Resync Read */
               4};    /* Home Node Pot Write */
 
/* Migration high priority table indexed by type of request */
 
static const int migrationhighprioritytable[] =
              {2,     /* Normal Page Read */
               2,     /* Directory Write */
               1,     /* Checkpoint Header Write */
               2,     /* Migrate Read */
               2,     /* Migrate Write */
               2,     /* Journal Write */
               6,     /* Resync Read */
               2};    /* Home Node Pot Write */
 
 
/* Externally visible variables */
 
const int *prioritytable = normalprioritytable;
 
 
/* Local static variables */
 
static
int migratetransitcount = 0;    /* Outstanding migrate operations */
static struct Request *restartrequest = NULL;
static
int domaincallmigrateactive = 0; /* Domain call level rtn active */
static NODE *actor;             /* The active domain or NULL */
 
 
    /* Following fields used in gmiimpr and gmidmpr */
 
static int migrationpriorityreasons = 0;  /* Reasons for high prty */
static uint64 migrationhighprioritystart;    /* time at start of hi prty */
 
 
    /* Following fields used in do_migrate0 */
 
static
int nextmigrate0phase = 1;      /* Phase number of next Migrate_Wait */
                                /* phase (1, 2, or 3) */
static
int anallocationpotchanged = 0; /* Flag for changed allocation pot */
static
int hinotincorerangelocindex = 0;
#define MAXNOTINCORERANGELOC 20
static RANGELOC notincorerangeloc[MAXNOTINCORERANGELOC];
static CDA lowcda, hicda;
 
 
    /* Following fields used in do_migrate1 and subroutines */
 
static
int nextmigrationphase = 1;     /* Phase number of next */
                                /* Migrate_MigrateThese */
                                /* phase (1, 2, 3, 4, 5, or 6) */
static
CTE *homenodepots = NULL;       /* Home node pot list for migration */
static int badhomesindex = 0;
static RANGELOC *badhomes;      /* To remember bad home nodepots */
static int tobemigratedindex = 0;
static CTE **tobemigratedaddr;  /* CTEs needing migration */
static char *pagequeued;        /* input already handled */
#define pagequeuedsize (pagesize/6)



/* Prototypes for internal routines */

static int queueformigrateoutput(CTE *cte);

 
 
/*********************************************************************
imigrate - Initialize the migrate module
 
  Input - 
     data   - Space for gmigratc's work areas
     len    - The length of space provided
 
  Output - None
*********************************************************************/
void imigrate(char *data, uint32 len)
{
   if (len < (sizeof(RANGELOC) * (pagesize/6)
              + sizeof(CTE*) * (pagesize/6)
              + (pagesize/6)))
      crash("GMIGRATC001 Not enough work area space provided");
   badhomes = (RANGELOC *)data;
   data += sizeof(RANGELOC) * (pagesize/6);
   tobemigratedaddr = (CTE **)data;
   data += sizeof(CTE *) * (pagesize/6);
   pagequeued = data;
   memzero(pagequeued, pagequeuedsize);
} /* End imigrate */
 
 
/*********************************************************************
queueactorifany - Put actor on a aueue and set actor to NULL
 
  Input - 
     queue   - Pointer the the queuehead
 
  Output - None
*********************************************************************/
static void queueactorifany(struct QueueHead *queue)
{
   if (!actor) return;
   enqueuedom(actor, queue);
   actor = NULL;
} /* End queueactorifany */
 
 
/*********************************************************************
migratetransitcountzero - Check transit count and handle nonzero
 
  Input - None
 
  Output -
     1 - transit count is zero, 0 - Non-zero, actor queued and set to NULL
*********************************************************************/
static int migratetransitcountzero(void)
{
   if (!migratetransitcount) return 1;
   queueactorifany(&migratetransitcountzeroqueue);
   checkforcleanstart();
   return 0;
} /* End migratetransitcountzero */
 
 
/*********************************************************************
backoutmigratetransitcount - Decrement counter
 
  Input - None
 
  Output - None
*********************************************************************/
static void backoutmigratetransitcount(void)
{
   if (!migratetransitcount)
      crash("GMIGRATEC001 migratetransitcount underflow");
   migratetransitcount--;
} /* End backoutmigratetransitcount */
 
 
/*********************************************************************
decrementmigratetransitcount - Decrement counter and run waiters if zero
 
  Input - None
 
  Output - None
*********************************************************************/
static void decrementmigratetransitcount(void)
{
   backoutmigratetransitcount();
   if (migratetransitcount) return;
   enqmvcpu(&migratetransitcountzeroqueue);
} /* End decrementmigratetransitcount */
 
 
/*********************************************************************
resetmigrateflags - Reset the migrate flags in a node pot
 
  Input -
     cte     - Pointer to the CTE of the node pot
 
  Output - None
*********************************************************************/
static void resetmigrateflags(CTE *cte)
{
   struct NodePot *p = (struct NodePot *)
                        map_window(QUICKWINDOW, cte, MAP_WINDOW_RW);
 
   memzero(p->migratedthismigration, sizeof p->migratedthismigration);
} /* End resetmigrateflags */
 
 
/*********************************************************************
gmiintnp - Update the integrity data in a node pot
 
  Input -
     cte     - Pointer to the CTE of the node pot
 
  Output - None
*********************************************************************/
void gmiintnp(CTE *cte)
{
   struct NodePot *p = (struct NodePot *)
                           map_window(QUICKWINDOW, cte, MAP_WINDOW_RW);
   uchar *cp = &p->checkbyte;
 
   p->disknodes[0].flags += DNINTEGRITYONE;
   p->checkbyte = (p->disknodes[0].flags & DNINTEGRITY)
                     | (~(*(cp-1)) & (UCHAR_MAX - DNINTEGRITY));
} /* End gmiintnp */
 
 
/*********************************************************************
putnodeinlimbo - Keep a node which can't be migrated in swap area
 
  Input -
     dn      - Pointer to the disk version of the node in the node pot
               Node pot must be tied down.
 
  Output -
     Returns non-zero if node placed in limbo, otherwise zero
*********************************************************************/
static int putnodeinlimbo(struct DiskNode *dn)
{
   CDA tempcda;
   NODE *nf;
 
   Memcpy(tempcda, dn->cda, sizeof(CDA));
   tempcda[0] &= 0x7f;
   nf = srchnode(tempcda);
   if (!nf) {
      if (!gdiciicd(dn->cda)) {  /* Not in working directory */
         nf = getmntf(dn);
         if (!nf) return 0;
      } else {                   /* In working directory --> limboed */
         gdirembk(nf->cda, TRUE);
         return 1;
      }
   }
   nf->flags |= NFDIRTY;      /* Ensure node written in next ckpt */
   gdirembk(nf->cda, TRUE);
   return 1;
} /* End putnodeinlimbo */
 
 
/*********************************************************************
putpageornodeinlimbo - Ensure page or node written to working swap area
 
  Input -
     cte     - Pointer to the CTE of the page to output
 
  Output -
     Returns non-zero if page or node placed in limbo, otherwise zero
*********************************************************************/
static int putpageornodeinlimbo(CTE *cte)
{
   switch (cte->ctefmt) {
    case NodePotFrame:
      corelock_page(cte);
      {
         struct NodePot *p = (struct NodePot *)
                           map_window(IOSYSWINDOW, cte, MAP_WINDOW_RW);
         struct DiskNode *dn = p->disknodes;
         int i;

         for (i = 0; i < NPNODECOUNT; (dn++, i++)) {
            if (p->migratedthismigration[i]) {
               if (!putnodeinlimbo(dn)) {
                  coreunlock_page(90,cte);
                  return 0;
               }
               p->migratedthismigration[i] = 0;
            }
         }
      }
      coreunlock_page(91,cte);
      cte->flags &= ~ctchanged;    /* Reset changed bit */
      break;
    case AlocPotFrame:
      break;    /* grtfapm will fix it */
    case PageFrame:
      gdirembk(cte->use.page.cda, TRUE);
      cte->flags |= ctchanged;
      break;
    default:
      crash("GMI823 put in limbo strange ctefmt");
   }
   return 1;
} /* End putpageornodeinlimbo */
 
 
/*********************************************************************
migratenodepot - Migrate the nodes in a node pot
 
  Input -
     cte     - Pointer to the CTE of the page to output
 
  Output -
     Returns non-zero if all nodes migrated, otherwise zero
*********************************************************************/
static int migratenodepot(CTE *scte)
{
   int allmigrated = 1;     /* Everything so far is migrated */
   struct NodePot *sp = (struct NodePot *)
                        map_window(CKPMIGWINDOW, scte, MAP_WINDOW_RO);
   struct DiskNode *sdn;
   int i;
 
   corelock_page(scte);
   sdn = sp->disknodes;
   for (i = 0; i < NPNODECOUNT; (sdn++, i++)) {
      RANGELOC rl;
      CTE *hcte;                    /* Home pot cte */
 
      rl = gdilkunm(sdn->cda);
      if ((rl.range || rl.offset)  /* CDA is in unmigrated directory */
           && (hcte = getfpic(rl)) /* And it's directory pot in core */
           && hcte == scte) {      /* And it's the input pot */
         rl = grtcrl(sdn->cda);      /* Get rangeloc of home pot */
         if (rl.range == -1) {       /* Range is not mounted */
            allmigrated &= putnodeinlimbo(sdn);
         } else {                    /* Range is mounted */
            hcte = getfpic(rl);
            if   (hcte                 /* Home pot is locked in core */
                  && hcte->extensionflags & ctkernellock) {
               struct NodePot *hp = (struct NodePot *)
                        map_window(QUICKWINDOW, hcte, MAP_WINDOW_RW);
               struct DiskNode *hdn;
               uint32 offset = b2long(sdn->cda+2, 4)
                          - b2long(hp->disknodes[0].cda+2,4);

               hdn = hp->disknodes + offset;
               hp->migratedthismigration[offset] = 1;
               if (Memcmp(sdn->cda, hdn->cda, sizeof(CDA)))
                  crash("GMIGRATC024 Wrong migration node");
               *hdn = *sdn;
               gdirembk(sdn->cda, FALSE);
            } else {                  /* Home pot not locked in core */
               int ndx;
               for (ndx = 0; ndx < badhomesindex; ndx++) {
                  if  (badhomes[ndx].range == rl.range
                       && badhomes[ndx].offset == rl.offset) {
                     allmigrated &= !putnodeinlimbo(sdn);
               break;
                  }
               }
            }
         }
      }
   }
   coreunlock_page(92,scte);
   return allmigrated;
} /* End migratenodepot */
 
 
/*********************************************************************
homefetchended - Process ending of a fetch home node pot operation
 
  Input -
     req     - Pointer to the request that ended
 
  Output - None
*********************************************************************/
static void homefetchended(REQUEST *req)
{
   if (getendedcleanup(req) >= 1) {   /* Successful read */
      CTE *cte = req->pagecte;
 
      cte->use.pot.nodepottype = nodepottypehomemigr;
      numbernodepotsincore[nodepottypehomemigr]++;
      getsucnp(cte, req);        /* Set up the node pot */
      cte->use.pot.homechain = homenodepots;  /* Add to home chain */
      homenodepots = cte;
      cte->corelock = 0;        /* coreunlock_node w/ old LRU */
      resetmigrateflags(cte);   /* No nodes yet migrated this time */
   }
   getrereq(req);
   decrementmigratetransitcount();
} /* End homefetchended */
 
 
/*********************************************************************
backupfetchended - Process ending of a fetch backup area
 
  Input -
     req     - Pointer to the request that ended
 
  Output - None
*********************************************************************/
static void backupfetchended(REQUEST *req)
{
   CTE *cte = req->pagecte;
 
   if (getendedcleanup(req) == 0)
      crash("GMIGRATC020 Could not read backup swap area");
   cte->corelock = 0;        /* coreunlock_node w/ old LRU */
   if (req->pcfa.flags & REQPOT) {
      if (req->pcfa.flags & REQALLOCATIONPOT)
         crash("GMIGRATC021 Allocation pot read from backup swap area");
      cte->use.pot.nodepottype = nodepottypeswapcleanmigr;
      numbernodepotsincore[nodepottypeswapcleanmigr]++;
      getsucnp(cte, req);        /* Set up the node pot */
      if (!migratenodepot(cte)) {   /* Nodepot not all migrated */
         tobemigratedaddr[tobemigratedindex++] = cte;
      } else {
         if (!(cte->extensionflags & ctkernellock))
            crash("GMIGRATC022 backupfetchended cte not kernel locked");
         cte->extensionflags &= ~ctkernellock;
      }
   } else {                 /* A page frame */
      if (setupversion(cte, req)) {
         if (!(cte->flags & ctbackupversion)) {  /* Current version */
            if (!queueformigrateoutput(cte))
               tobemigratedaddr[tobemigratedindex++] = cte;
         } else cte->extensionflags &= ctkernellock;  /* Backup vers */
      }
   }
   getrereq(req);
   decrementmigratetransitcount();
} /* End backupfetchended */
 
 
/*********************************************************************
allocationpotreadended - Process ending of a allocation pot read
 
  Input -
     req     - Pointer to the request that ended
 
  Output - None
*********************************************************************/
static void allocationpotreadended(REQUEST *req)
{
   getended(req);
   decrementmigratetransitcount();
} /* End allocationpotreadended */
 
 
/*********************************************************************
migrateoutputended - Process ending of a migrate write operation
 
  Input -
     req     - Pointer to the request that ended
 
  Output - None
*********************************************************************/
static void migrateoutputended(REQUEST *req)
{
   CTE *cte = req->pagecte;
   int completed = 0;
   DEVREQ *drq;
 
   for (drq = req->devreqs; drq; drq = req->devreqs) {
      if (DEVREQCOMPLETE != drq->status) { /* Write not complete */
         grtmro(drq->device, cte);
      } else {                  /* Write completed normally */
         completed++;
         gbadrewt(drq->device, drq->offset);
      }
      req->devreqs = drq->devreq;
      getredrq(drq);
   }
   if (completed
       || AlocPotFrame != cte->ctefmt) {
      if (!(cte->extensionflags & ctkernellock))
         crash("GMIGRATC019 tobemigrated cte not kernel locked");
      cte->extensionflags &= ~ctkernellock;
   }
   if (completed) {            /* Some completed normally */
      if (PageFrame == cte->ctefmt) gdirembk(cte->use.page.cda, FALSE);
   } else {                    /* Nothing completed normally */
      if (AlocPotFrame == cte->ctefmt) {   /* Allocation pot */
         /* Possibly could call gdifapm */
         crash("GMIGRATC022 Couldn't write allocation pot");
      } else {
         if (!putpageornodeinlimbo(cte))
            crash("GMIGRATC021 Can't limbo a page or nodepot");
      }
   }
   if (PageFrame == cte->ctefmt) {  /* End of a page write */
      /* Release outstandingio lock */
      getunlok(cdahash(cte->use.page.cda));
   }
   getrereq(req);
   decrementmigratetransitcount();
} /* End migrateoutputended */
 
 
/*********************************************************************
readhomepot - Set up a read for a home node pot
 
  Input -
     cda     - Pointer to a cda in the pot to be read
 
  Output -
     Code as defined below:
*********************************************************************/
#define rrc_continue       0 /* Continue reading, actor not queued */
                             /* or actor queued & NULLed */
#define rrc_quit           1 /* Don't  read more, actor queued & NULLed */
static int readhomepot(CDA cda)
{
   struct CodeIOret ioret;
 
   migratetransitcount++;
   ioret = getreqhp(cda, REQMIGRATEREAD, homefetchended, actor);
   switch (ioret.code) {
    case io_notmounted:
      backoutmigratetransitcount();
      return rrc_continue;
    case io_notreadable:
      backoutmigratetransitcount();
      badhomes[badhomesindex++] = ioret.ioret.rangeloc;
      return rrc_continue;
    case io_potincore:
      backoutmigratetransitcount();
      if (!(ioret.ioret.cte->extensionflags
            & ctkernellock)) {
         ioret.ioret.cte->extensionflags |= ctkernellock;
         ioret.ioret.cte->use.pot.homechain = homenodepots;
         homenodepots = ioret.ioret.cte;
         resetmigrateflags(ioret.ioret.cte);
      }
      return rrc_continue;
    case io_started:
      migrateio++;
      actor = NULL;
      return rrc_continue;
    case io_cdalocked:
      backoutmigratetransitcount();
      actor = NULL;
      return rrc_continue;
    case io_noioreqblocks:
      backoutmigratetransitcount();
      actor = NULL;
      return rrc_quit;
   } /* End switch on getreqhp return code */
   crash("GMIGRATC007 Bad return from getreqhp");
} /* End readhomepot */
 
 
/*********************************************************************
readbackuppot - Set up a read for a backup node pot
 
  Input -
     cda     - Pointer to a cda in the pot to be read
 
  Output -
     Code as defined below:
        rrc_continue       0    Continue reading, actor not queued
                                or actor queued & NULLed
        rrc_quit           1    Don't continue reading, actor queued
*********************************************************************/
static int readbackuppot(CDA cda)
{
   struct CodeIOret ioret;
 
   migratetransitcount++;
   ioret = getreqnm(cda, REQMIGRATEREAD, backupfetchended, actor);
   switch (ioret.code) {
    case io_notmounted:
    case io_notreadable:
    case io_notindirectory:
      backoutmigratetransitcount();
      return rrc_continue;
    case io_potincore:
      backoutmigratetransitcount();
      migratenodepot(ioret.ioret.cte);
      return rrc_continue;
    case io_started:
      migrateio++;
      actor = NULL;
      if (restartrequest) {    /* Sychronous I/O on read already done */
         logicalpageio++;
         migrateio += restartrequest->completioncount;
         gddenq(restartrequest);
         restartrequest = NULL;
      }
      return rrc_continue;
    case io_cdalocked:
      backoutmigratetransitcount();
      actor = NULL;
      return rrc_continue;
    case io_noioreqblocks:
      backoutmigratetransitcount();
      actor = NULL;
      return rrc_quit;
   } /* End switch on getreqhp return code */
   crash("GMIGRATC008 Bad return from getreqmn");
} /* End readbackuppot */
 
 
/*********************************************************************
readbackuppage - Set up a read for a backup page
 
  Input -
     cda     - Pointer to the cda of the page to be read
 
  Output -
     Code as defined below:
        rrc_continue       0    Continue reading, actor not queued
                                or actor queued & NULLed
        rrc_quit           1    Don't continue reading, actor queued
*********************************************************************/
static int readbackuppage(CDA cda)
{
   struct CodeIOret ioret;
 
   migratetransitcount++;
   ioret = getreqpm(cda, REQMIGRATEREAD, backupfetchended, actor);
   switch (ioret.code) {
    case io_notmounted:
    case io_notreadable:
      backoutmigratetransitcount();
      return rrc_continue;
    case io_started:
      migrateio++;
      actor = NULL;
      if (restartrequest) {    /* Sychronous I/O on read already done */
         logicalpageio++;
         migrateio += restartrequest->completioncount;
         gddenq(restartrequest);
         restartrequest = NULL;
      }
      return rrc_continue;
    case io_cdalocked:
      backoutmigratetransitcount();
      actor = NULL;
      return rrc_continue;
    case io_noioreqblocks:
      backoutmigratetransitcount();
      actor = NULL;
      return rrc_quit;
   } /* End switch on getreqhp return code */
   crash("GMIGRATC008 Bad return from getreqmn");
} /* End readbackuppot */
 
 
/*********************************************************************
findallocationpot - Find and update an allocation pot
 
  Input -
     cda   - Pointer to a CDA in the allocation pot to find
 
  Output - 
     If the pot is mounted, the cda that is one higher than covered by
     the pot, otherwise the next higher mounted cda.
*********************************************************************/
static uchar *findallocationpot(const uchar *cda)
{
   static struct GrtAPI_Ret api;
   CTE *cte = NULL;
 
   api = grtapi(cda);
   if (-1 != api.rangeloc.range) {  /* Range is mounted */
 
      /* Scan allocation pot chain for this pot */
 
      for (cte = allocationpotchainhead; cte; cte = cte->hashnext) {
         if (cte->use.pot.potaddress.range == api.rangeloc.range &&
               cte->use.pot.potaddress.offset == api.rangeloc.offset)
         break;
      }
      if (cte) {            /* Found allocation pot in memory */
         if (api.resync) {
            cte->flags |= ctchanged;
            anallocationpotchanged = 1;
            cte->extensionflags |= ctkernellock;
         }
      } else {              /* Allocation pot not in memory */
         if (grtrlr(api.rangeloc)) {
            if (hinotincorerangelocindex < MAXNOTINCORERANGELOC) {
               notincorerangeloc[hinotincorerangelocindex++]
                               = api.rangeloc;
            }
            return api.highcda;
         }
      }
   }
   anallocationpotchanged |= gdicoboc(api.lowcda, api.highcda, cte);
   return api.highcda;
} /* End findalloctionpot */
 
 
struct checkmigrationneeded_ret {
   int code;                  /* Return code as follows: */
#define checkmigrationneeded_unchangedpage  0
#define checkmigrationneeded_unchangednode  1
#define checkmigrationneeded_unchangednotin 2
#define checkmigrationneeded_changed        3
   union {
      CTE *cte;                 /* iff unchangedpage */
      NODE *node;               /* iff unchangednode */
   } value;
};
/*********************************************************************
checkmigrationneeded - See if CDA has changed (changed ones need not be
                       migrated
 
  Input -
     cda     - Pointer to the CDA to check
 
  Output - None
*********************************************************************/
static struct checkmigrationneeded_ret checkmigrationneeded(CDA cda)
{
   struct checkmigrationneeded_ret ret;
 
   if (!gdiciicd(cda)) {            /* Not in working directory */
      if (cda[0] & 0x80) {          /* It is a node */
         NODE *nf;
         CDA lcda;
 
         Memcpy(lcda, cda, sizeof(CDA));
         lcda[0] &= 0x7f;           /* Turn off node bit in copy */
         nf = srchnode(lcda);
         if (!nf) {
            ret.code = checkmigrationneeded_unchangednotin;
            return ret;
         }
         if (nf->flags & NFDIRTY) {
            ret.code = checkmigrationneeded_changed;
            return ret;
         }
         ret.code = checkmigrationneeded_unchangednode;
         ret.value.node = nf;
         return ret;
      } else {                        /* It is a page */
         CTE *cte = srchpage(cda);
         if (!cte) {
            ret.code = checkmigrationneeded_unchangednotin;
            return ret;
         }
         if (cte->flags & ctchanged) {
            ret.code = checkmigrationneeded_changed;
            return ret;
         }
         /* N.B. There may be changed bits on in map entries for
            this page; that's ok; noticing the changed bit
            is just an optimization. */
         ret.code = checkmigrationneeded_unchangedpage;
         ret.value.cte = cte;
         return ret;
      }
   }
   ret.code = checkmigrationneeded_changed;
   return ret;
} /* End checkmigrationneeded */
 
 
/*********************************************************************
queueformigrateoutput -  Build and queue output I/O request
 
  Input -
     cte     - Pointer to the CTE of the page to output
 
  Output -
     Returns zero if actor queued, otherwise non-zero
*********************************************************************/
static int queueformigrateoutput(CTE *cte)
{
   struct CodeGRTRet hwl;
   REQUEST *req;
 
   hwl = grthomwl(cte);
   if (hwl.code == grt_notmounted) {  /* Home not mounted */
      if (PageFrame == cte->ctefmt) {   /* A page */
         if (!(cte->extensionflags & ctkernellock))
            crash("GMIGRATC015 tobemigrated cte not kernel locked");
         cte->extensionflags &= ~ctkernellock;
      }
      if (putpageornodeinlimbo(cte)) return 1;     /* In limbo */
      if (actor) {
         queueactorifany(&nonodesqueue); /* Try again */
         actor = NULL;
      }
      return 0;
   }
   req = acquirerequest();
   if (!req) {
      queueactorifany(&noiorequestblocksqueue);
      return 0;
   }
   req->potaddress = hwl.ioret.readinfo.id.potaddress;
   req->completioncount = 0;
   req->doneproc = migrateoutputended;
   req->type = REQMIGRATEWRITE;
   req->pagecte = cte;
   switch (cte->ctefmt) {
    case NodePotFrame:
    case AlocPotFrame:
      req->type = REQHOMENODEPOTWRITE;   /* Fix request type */
      break;;
    case PageFrame:
      if (!getlock(cdahash(cte->use.page.cda), actor)) {
         actor = NULL;
         getrereq(req);
         return 0;                  /* Return - could not queue */
      }
      break;;
    default:
      crash("GMI763 qfmo invalid ctefmt");
   }
   for (;;) {
      DEVREQ *drq = acquiredevreq(req);
 
      if (!drq) {                   /* Ran out of devreqs, try later */
         for (drq = req->devreqs; drq; drq = req->devreqs) {
            req->devreqs = drq->devreq;
            getredrq(drq);
         }
         queueactorifany(&noiorequestblocksqueue);
         if (cte->ctefmt == PageFrame) {  /* Need to unlock outstandingio */
            getunlok(cdahash(cte->use.page.cda));
         }
         getrereq(req);
         return 0;
      }
      drq->device = hwl.ioret.readinfo.device;
      drq->offset = hwl.ioret.readinfo.offset;
      md_dskdevreqaddr(drq);
      req->completioncount++;
      hwl.ioret.readinfo = grtnext();
      if (!hwl.ioret.readinfo.device) {   /* End of write locs */
         migratetransitcount++;
         logicalpageio++;
         migrateio += req->completioncount;
         if (domaincallmigrateactive) restartrequest = req;
         else gddenq(req);
         return 1;
      }
   }
} /* End queueformigrateoutput */
 
 
/*********************************************************************
migratetobemigratedchain -
                       migrated
 
  Input - None
   
  Output -
     Returns zero if actor queued, otherwise non-zero
 
  Notes:
     Entries in the "TOBEMIGRATEDCHAIN" are CORETBENs for pages or
       backup node pots.  The page frame has CTKERNELLOCK on.
     The page or pot is in the PAGCHHD hash chains.
     If a page, it is the current version.
*********************************************************************/
static int migratetobemigratedchain(void)
{
   int newindex = 0;
   int i;
   CTE *cte;
 
   for (i = 0; i < tobemigratedindex; i++) {
      cte = tobemigratedaddr[i];
      if (NodePotFrame == cte->ctefmt) {
         if (migratenodepot(cte)) {     /* all migrated */
            if (!(cte->extensionflags & ctkernellock))
               crash("GMIGRATC011 tobemigrated cte not kernel locked");
            cte->extensionflags &= ~ctkernellock;
            cte->corelock = 0;      /* coreunlock_page with low lru */
         } else {                   /* Not all migrated, keep around */
            tobemigratedaddr[newindex++] = cte;
         }
      } else {
         if (PageFrame != cte->ctefmt)
            crash("GMIGRATC012 Non-page/nodepot on tobemigrated chain");
         if (!(cte->flags & ctchanged)) { /* Must write clean page */
         /* N.B. There may be changed bits on in map entries for
            this page; that's ok; noticing the changed bit
            is just an optimization. */
            if (!queueformigrateoutput(cte)) {
               tobemigratedaddr[newindex++] = cte;
            }
         } else {             /* Changed pages need not be migrated */
            if (!(cte->extensionflags & ctkernellock))
               crash("GMIGRATC013 tobemigrated cte not kernel locked");
            cte->extensionflags &= ~ctkernellock;
            gdirembk(cte->use.page.cda, TRUE);
         }
      }
   }
   tobemigratedindex = newindex;
   if (actor) return 1;
   return 0;
} /* End migratetobemigratedchain */
 
 
/*********************************************************************
do_migrate0 - Perform Migrate_Wait logic
 
  Input - None
 
  Output -
     Return code for jumpee or KT if actor queued
*********************************************************************/
uint32 do_migrate0(void)
{
   actor = cpuactor;
 
   switch (nextmigrate0phase) {
phase1:
    case 1:  /* Check if all pages and nodes have been migrated */
      if (gdiesibd()) {           /* Not all have been migrated */
         uint32 rc = (NUMBEROFREQUESTS * 5) / 8;
         if (rc > (endmemory - first_user_page)/2)
            rc = (endmemory - first_user_page)/2;
         if (rc < 7) rc = 7;
         return rc;            /* Pages to use in migration */
      }
      /* unmigrated directory is empty */
      nextmigrate0phase = 2;    /* Set phase and fall thru into it */
 
    case 2:  /* Migrate allocation pots */
      for (;;) {                /* Read and update until done */
         const uchar *cda;
         int i;
 
         anallocationpotchanged = 0;
         hinotincorerangelocindex = 0;
         memzero(lowcda, sizeof(CDA));
         memzero(hicda, sizeof(CDA));
         cda = cdaone;
         for (cda = gdifncda(cda); cda; cda = gdifncda(cda))
            cda = findallocationpot(cda);
         gswckmp();    /* Test swap space migration priority off */
         if (anallocationpotchanged
             || hinotincorerangelocindex == 0) {
            CTE *cte;
 
            for (cte = allocationpotchainhead;
                 cte; cte = cte->hashnext) {
               if (cte->flags & ctchanged) {  /* Changed alloc pot */
                  grtintap(cte);             /* Update integrity data */
                  cte->flags &= ~ctchanged;  /* Off changed bit */
                  if (!queueformigrateoutput(cte)) {
                     cte->flags |= ctchanged;  /* On changed bit */
                     return KT;
                  }
                  anallocationpotchanged = 1;
               }
            }
            if (!anallocationpotchanged) { /* All pots migrated */
               if (gdiesibd()) {     /* unmigrated --> range dismount */
                  nextmigrate0phase = 1;  /* Back to phase 1 */
                  goto phase1;
               }
               nextmigrate0phase = 3;  /* Go on to phase 3 */
      break;
            } else {
               if (!migratetransitcountzero()) return KT;
      continue;
            }
         }
            /* Non allocation pots need to be written, read some */
               /* Read as many allocation pots as possible */
 
         for (i = 0; i < hinotincorerangelocindex; i++) {
            struct CodeIOret ret;
 
            migratetransitcount++;
            ret = getreqap(notincorerangeloc[i],
                           REQMIGRATEREAD, allocationpotreadended,
                           actor);
            switch (ret.code) {
             case io_notmounted:
               crash("GMIGRATC002 io_notmounted reading alloc pot");
             case io_notreadable:
               crash("GMIGRATC003 io_notreadable reading alloc pot");
             case io_started:
               actor = NULL;
               migrateio++;
               break;
             case io_cdalocked:
               backoutmigratetransitcount();
               actor = NULL;            /* Leave him on I/O queue */
               break;
             case io_noioreqblocks:     /* Queued on noblocks queue */
               backoutmigratetransitcount();
               return KT;
            }
         }
         if (!actor) {               /* If actor queued */
            return KT;
         }
         if (!migratetransitcountzero()) return KT;
      break;           /* All read - on to phase 3 */
      }     /* breaking out of forever loop goes to phase 3 */
    case 3:  /* Set time stamps on ranges with partial migration */
       if (!gupdpdr(actor)) {   /* Couldn't update PDRs */
          return KT;
       }
 
       /* Migration finished */
 
       iosystemflags &= ~MIGRATIONINPROGRESS;
       if (lowcoreflags.prtckpt) consprint("migr done ");
       if (iosystemflags & CHECKPOINTATENDOFMIGRATION) {
          iosystemflags &= ~CHECKPOINTATENDOFMIGRATION;
          gcktkckp(TKCKPCKPTAFTERMIGR);
       }
       else enqmvcpu(&resyncqueue); /* proceed with resyncing */
       nextmigrate0phase = 1;
       completedmigrations++;
       enqueuedom(cpuactor, &migratewaitqueue);
       return KT;
   }
return 0;} /* End do_migrate0 */
 
 
/*********************************************************************
do_migrate1 - Perform Migrate_MigrateThese logic
 
  Input - None
 
  Output -
     Return code for jumpee or KT if actor queued
*********************************************************************/
uint32 do_migrate1(void)
{
   int parametercount = cpuarglength / 6;
   CTE *cte;
 
   actor = cpuactor;
   if (cpuarglength % 6) {
      checkforcleanstart();
      return 1;              /* Return code 1 */
   }
 
   switch (nextmigrationphase) {
      int i;
      uchar *cdaptr;
      CDA cda;
 
    case 1:   /* Build input requests for home node pots */
      cdaptr = (uchar*)cpuargaddr;
      for (i = parametercount-1; i >= 0; (cdaptr+=sizeof(cda), i--)) {
         if (cpumempg[0] || cpuexitblock.argtype != arg_memory) {
            Memcpy(cda, cdaptr, sizeof(cda));
         } else if ( 0 == movba2va(cda, cdaptr, sizeof(cda)) ) {
            crash("GMIGRATC005 - Overlap with stack?");
         }
         if (cda[0] & 0x80 && !pagequeued[i]) {
            struct checkmigrationneeded_ret ret;
 
            ret = checkmigrationneeded(cda);
            switch (ret.code) {
 
             case checkmigrationneeded_unchangedpage:
               crash("GMIGRATC004 Internal error");
             case checkmigrationneeded_unchangednode:
             case checkmigrationneeded_unchangednotin:
               switch (readhomepot(cda)) {
                case rrc_quit:
                  return KT;           /* Actor already queued */
                case rrc_continue:
                  pagequeued[i] = 1;
               } /* End switch on readhomepot return code */
               break;
             case checkmigrationneeded_changed:
               pagequeued[i] = 1;
            } /* End switch on return from checkmigrationneeded */
         } /* End need to process this cda */
      } /* End for all input cdas */
      if (!actor) {
         return KT;               /* Actor already queued */
      }  /* Otherwise fall through into the next state */
      nextmigrationphase = 2;
 
    case 2:   /* Wait for I/O completion and then continue */
      if (!migratetransitcountzero()) return KT;  /* Wait for all I/O */
      memzero(pagequeued, pagequeuedsize);  /* Clear page queued */
         /* Fall through into the next state */
      nextmigrationphase = 3;
 
    case 3:   /* Bring in backup pages and node pots & move them home */
      domaincallmigrateactive = 1;
      cdaptr = (uchar *)cpuargaddr;
      for (i = parametercount-1; i >= 0; (cdaptr+=sizeof(cda), i--)) {
         if (cpumempg[0] || cpuexitblock.argtype != arg_memory) {
            Memcpy(cda, cdaptr, sizeof(cda));
         } else if ( 0 == movba2va(cda, cdaptr, sizeof(cda)) ) {
            crash("GMIGRATC010 - Overlap with stack?");
         }
         if (!pagequeued[i]) {
            struct checkmigrationneeded_ret ret;
 
            ret = checkmigrationneeded(cda);
            switch (ret.code) {
               RANGELOC rl;
               int rrc_ret;
 
             case checkmigrationneeded_unchangedpage:
               rl = gdilkunm(cda);
               if (rl.range != 0) {  /* rangeloc returned */
                  if (ret.value.cte->extensionflags & ctkernellock)
                     crash("GMIGRATC009 Page already locked");
                  ret.value.cte->extensionflags |= ctkernellock;
                  tobemigratedaddr[tobemigratedindex++]
                        = ret.value.cte;
               }
               pagequeued[i] = 1;
               break;
             case checkmigrationneeded_unchangednode:
             case checkmigrationneeded_unchangednotin:
               if (cda[0] & 0x80) {     /* If it is a node */
                  rrc_ret = readbackuppot(cda);
               } else {         /* read backup page */
                  rrc_ret = readbackuppage(cda);
               }
               switch (rrc_ret) {
                case rrc_quit:
                  domaincallmigrateactive = 0;
                  return KT;           /* Actor already queued */
                case rrc_continue:
                  pagequeued[i] = 1;
               }
               break;
             case checkmigrationneeded_changed:
               gdirembk(cda, TRUE);    /* Changed --> don't migrate */
               pagequeued[i] = 1;
            } /* End switch on return from checkmigrationneeded */
         } /* End need to process this cda */
      } /* End for all input cdas */
      domaincallmigrateactive = 0;
      if (!migratetobemigratedchain() || !actor) {
         return KT;               /* Actor already queued */
      }  /* Otherwise fall through into the next state */
      nextmigrationphase = 4;
 
    case 4:   /* Wait for I/O completion and then continue */
      if (!migratetobemigratedchain()) {
         return KT;               /* Actor already queued */
      }  /* Otherwise fall through into the next state */
      if (!migratetransitcountzero()) return KT;  /* Wait for all I/O */
      if (tobemigratedindex) {                 /* Still stuff to move */
         enqueuedom(cpuactor, &migratetransitcountzeroqueue);
         return KT;
      }
         /* Fall through into the next state */
      nextmigrationphase = 5;
 
    case 5:  /* Queue updated home node pots for output */
      for (cte = homenodepots; cte; cte = homenodepots) {
         gmiintnp(cte);          /* Update integrity data in nodepot */
            /* Keep gspace from cleaning page & allow quick steal */
         cte->flags &= ~(ctchanged | ctreferenced);
         /* ASSEMBLER code checked CDAs for ascending order here */
         if (!queueformigrateoutput(cte)) {
            return KT;             /* Actor appropreatly queued */
         }
         homenodepots = cte->use.pot.homechain;
      }
         /* Fall through into the next state */
      nextmigrationphase = 6;
 
    case 6:   /* Wait for I/O completion and complete jump */
      if (!migratetransitcountzero()) return KT; /* Wait for all I/O */
      memzero(pagequeued, pagequeuedsize);  /* Clear page queued */
      badhomesindex = 0;                      /* Set up for next call */
      nextmigrationphase = 1;
      checkforcleanstart();
      return 0;
 
    default: crash("GMIGRATC006 do_migrate1 state error");
   }
} /* End do_migrate1 */
 
 
/*********************************************************************
gmiimpr - Increment reasons for high priority migration
 
  Input - None
 
  Output - None
*********************************************************************/
void gmiimpr(void)
{
   migrationpriorityreasons++;
   if (migrationpriorityreasons > 3) /* Can't be more than 3 reasons */
      /* Only reasons are:  key call, directory space, and swap space */
      crash("GMIGRATC022 migrationpriorityreasons overflow");
   if (migrationpriorityreasons > 1) return; /* Was at high prty */
   migrationhighprioritystart = read_system_timer();
   prioritytable = migrationhighprioritytable;
   migrationpriority = 1;
} /* End gmiimpr */
 
 
/*********************************************************************
gmidmpr - Decrement reasons for high priority migration
 
  Input - None
 
  Output - None
*********************************************************************/
void gmidmpr(void)
{
   uint64 time;
 
   migrationpriorityreasons--;
   if (migrationpriorityreasons < 0)
      crash("GMIGRATC023 migrationpriorityreasons underflow");
   if (0 != migrationpriorityreasons) return; /* Still at high prty */
   prioritytable = normalprioritytable;
   migrationpriority = 0;
   time = read_system_timer();
   //llisub (&time, &migrationhighprioritystart);
   time -= migrationhighprioritystart;
   //lliadd (&highmigrationprioritytime, &time);
   *(uint64*)&highmigrationprioritytime += time;
} /* End gmidmpr */
