/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* Get - External interface to get pages & nodes from disk */
 
#include "keyh.h"
#include "disknodh.h"
#include "cpujumph.h"
#include "locksh.h"
#include "sysdefs.h"
#include "spaceh.h"
#include "getih.h"
#include "wsh.h"
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include "lli.h"
#include "sysdefs.h"
#include "keyh.h"
#include "disknodh.h"
#include "cpujumph.h"
#include "locksh.h"
#include "wsh.h"
#include "spaceh.h"
#include "queuesh.h"
#include "ioreqsh.h"
#include "ioworkh.h"
#include "geteh.h"
#include "getih.h"
#include "get2gclh.h"
#include "grangeth.h"
#include "grt2geth.h"
#include "gdirecth.h"
#include "gdi2geth.h"
#include "get2gmih.h"
#include "dskiomdh.h"
#include "memomdh.h"
#include "mi2mdioh.h"
#include "kerinith.h"
#include "cvt.h"
#include "kermap.h" /* for lowcoreflags */
#include "jpageh.h"
#include "getmntfh.h"
#include "consmdh.h"
#include "scafoldh.h"
#include "memutil.h"

/* Local static data */
 
static PCFA localpcfa;

#define NUMBEROFDEVREQS (NUMBEROFREQUESTS * 2)

static DEVREQ *devreqbase = NULL;    /* Head of DEVREQ free list */
static REQUEST *requestbase = NULL;  /* Head of REQUEST free list */
static REQUEST requestpool[NUMBEROFREQUESTS];
static DEVREQ devreqpool[NUMBEROFDEVREQS];
 
 
/*********************************************************************
iet - Initialize the get module 
  Input - None
   
  Output - None
*********************************************************************/
void iet(void)
{
   int i;
   
   buppage1.use.page.leftchain = (union Item *)&buppage1;
   buppage1.use.page.rightchain = (union Item *)&buppage1;
   buppage2.use.page.leftchain = (union Item *)&buppage2;
   buppage2.use.page.rightchain = (union Item *)&buppage2;

   for (i = 0; i<NUMBEROFREQUESTS; i++) {
      requestpool[i].doneproc = NULL;   /* To catch bugs */
      requestpool[i].next = requestbase;
      requestbase = requestpool+i;
   }
   for (i = 0; i<NUMBEROFDEVREQS; i++) getredrq(devreqpool+i);
   } /* End iet */
 
 
/*********************************************************************
checkforcleanstart - Continue checkpoint, or start cleaning
                     if needed.
 
  Input -
     dom   - Pointer to the domain's root node
     queue - Pointer to the queue head
 
  Output - None
*********************************************************************/
void checkforcleanstart(void)
{
   /* Loop doing things until nothing left to do. */
   while (
      ((continuecheckpoint ? (*continuecheckpoint)() : (void)0) ,
       (nodecleanneeded ? gspcleannodes() : FALSE))
      || (iosystemflags & PAGECLEANNEEDED ? gddstartpageclean() : FALSE)
      ) ; /* If did anything, repeat. */
} /* End checkforcleanstart */
 
 
/*********************************************************************
putdomainifanyonqueue - If domain passed is not NULL put it on queue
 
  Input -
     dom   - Pointer to the domain's root node
     queue - Pointer to the queue head
 
  Output - None
*********************************************************************/
static void putdomainifanyonqueue(NODE *dom, struct QueueHead *qh)
{
   if (dom) enqueuedom(dom, qh);
} /* End putdomainifanyonqueue */
 
 
/*********************************************************************
checkfreerequests - Check the free request list

  Input -
     req - Pointer to a request that should not be on the free list

  Output - None
*********************************************************************/
void checkfreerequests(REQUEST *req)
{  REQUEST *r;
   for (r = requestbase; r; r = r->next) {
      if (r == req) crash("GET990 req on free list");
   }
}


/*********************************************************************
acquirerequest - Get a REQUEST block from the free queue
 
  Input - None
 
  Output - A pointer to the request or NULL
*********************************************************************/
REQUEST *acquirerequest(void)
{
   REQUEST *req = requestbase;
 
   if (!req) {
      requestsout++;          /* Count the event */
      return NULL;
   }
   requestbase = req->next;
   req->devreqs = NULL;       /* Set up the request block */
   req->next = NULL;
   req->pcfa.flags = 0;
   checkfreerequests(req);
   return req;
} /* End acquirerequest */
 
 
/*********************************************************************
getrereq - Return a REQUEST block to the free queue
 
  Input -
     req - The request block to free
 
  Output - None
*********************************************************************/
void getrereq(REQUEST *req)
{
/*...   req->doneproc = NULL;*/   /* To catch bugs */
   checkfreerequests(req);
   req->next = requestbase;
   requestbase = req;
    /* Run domains blocked on no I/O request blocks */
   enqmvcpu(&noiorequestblocksqueue);
} /* End getrereq */
 
 
/*********************************************************************
checkfreedevreqs - Check the free devreq list

  Input -
     drq - Pointer to a devreq that should not be on the free list

  Output - None
*********************************************************************/
void checkfreedevreqs(DEVREQ *drq)
{  DEVREQ *r;
   for (r = devreqbase; r; r = r->devreq) {
      if (r == drq) crash("GET991 drq on free list");
   }
}


/*********************************************************************
acquiredevreq - Get and initialize a DEVREQ block from the free queue
 
  Input - None
 
  Output - A pointer to the request or NULL
 
  Conversion notes:
     Callers must set the drq->device and drq->offset fields and then
     call md_dskdevreqaddr(drq);
     The drq->offset was called addressondevice in assembler
*********************************************************************/
DEVREQ *acquiredevreq(REQUEST *req)
{
   DEVREQ *drq = devreqbase;
 
   if (!drq) {
      devreqsout++;           /* Count the event */
      return NULL;
   }
   devreqbase = drq->devreq;
   drq->flags = 0;            /* Set up the request block */
   drq->nextio = drq;         /* Chain to self */
   drq->previo = drq;
   drq->status = DEVREQOFFQUEUE;
   drq->request = req;        /* Chain devreq off the request */
   drq->devreq = req->devreqs;
   req->devreqs = drq;
   checkfreedevreqs(drq);
   return drq;
} /* End acquiredevreq */
 
 
/*********************************************************************
getredrq - Return a DEVREQ block to the free queue
 
  Input -
     drq - A  pointer to the devreq block to free
 
  Output - None
*********************************************************************/
void getredrq(DEVREQ *drq)
{
   checkfreedevreqs(drq);
   if (drq->flags & DEVREQSWAPAREA) gdiredrq(drq);
   *(uint_t *)&drq->request |= 0x80000000;  /* To catch bugs... */
   drq->devreq = devreqbase;
   devreqbase = drq;
} /* End getredrq */
 
 
/*********************************************************************
setuppots - Chain a pot on the hash chain passed
 
  Input -
     cte   - A pointer to the CTE for the pot
     chain - A pointer to the chain head
     req   - A pointer to the request
 
  Output - None
*********************************************************************/
static void setuppots(CTE *cte, CTE **chain, REQUEST *req)
{
   cte->hashnext = *chain;                    /* Chain with pots */
   *chain = cte;
   cte->use.pot.potaddress = req->potaddress; /* Set pot id */
   getredrq(req->devreqs);                    /* Return good devreq */
} /* End setuppots */
 
 
/*********************************************************************
setupnodepot - Chain a node pot on the correct hash chain
 
  Input -
     cte   - A pointer to the CTE for the pot
     req   - A pointer to the request
 
  Output - None
*********************************************************************/
static void setupnodepot(CTE *cte, REQUEST *req)
{
   setuppots(cte,
             apagechainheads + (pagechainhashmask &
                                    pothash(req->potaddress)),
             req);
   cte->ctefmt = NodePotFrame;
} /* End setupnodepot */
 
 
/*********************************************************************
getsucnp - Chain a node pot on the correct hash chain
 
  Input -
     cte   - A pointer to the CTE for the node pot
     req   - A pointer to the request
 
  Output - None
*********************************************************************/
void getsucnp(CTE *cte, REQUEST *req)
{
   setupnodepot(cte, req);
   if (!(cte->extensionflags & ctkernellock))
      crash("GETC001 CTE not kernel locked on call to getsucnp");
} /* End getsucnp */
 
 
/*********************************************************************
getfpic - Find a node pot in memory
 
  Input -
     rl  - The rangeloc or swaploc for the node pot
 
  Output -
     Pointer to CTE for pot or NULL if it is not in core
*********************************************************************/
CTE *getfpic(RANGELOC rl)
{
   register CTE *c;
 
   c = apagechainheads[(rl.range << 16 | rl.offset) &
                                                 pagechainhashmask];
   for (; c; c = c->hashnext) {
      if (rl.offset == c->use.pot.potaddress.offset
          && rl.range  == c->use.pot.potaddress.range
          && NodePotFrame  == c->ctefmt)
   break;
   }
   return c;
} /* End getfpic */
 
 
/*********************************************************************
getqnodp - Queue a node pot on the hash chains
 
  Input -
     cte - A pointer to the CTE for the node pot
 
  Output - None
*********************************************************************/
void getqnodp(CTE *cte)
{
   register CTE **cp;
 
   if (cte->ctefmt != NodePotFrame) crash("GET874 getqnodp not pot");
   cp = apagechainheads + (pothash(cte->use.pot.potaddress)
                                    & pagechainhashmask);
   cte->hashnext = *cp;         /* Place at head of hash chain */
   *cp = cte;
    /* Search rest of chain for duplicates */
   for (cp = &cte->hashnext; *cp; cp = &(*cp)->hashnext) {
      if (cte->use.pot.potaddress.offset ==
                               (*cp)->use.pot.potaddress.offset
          && cte->use.pot.potaddress.range  ==
                               (*cp)->use.pot.potaddress.range
          && NodePotFrame == cte->ctefmt) {
         CTE *duplicate = *cp;
         *cp = duplicate->hashnext;       /* Dechain entry */
         gspmpfa(duplicate);
         return;
      }
   }
} /* End getqnodp */
 
/*********************************************************************
getmntf - Move disknode to node frame
 
  Input -
     dnp - Pointer to the DiskNode to move to a node frame
 
  Output -
      Returns NULL if no node frames available
      Otherwise returns pointer to the node frame allocated
  Caller must call checkforcleanstart.
*********************************************************************/
NODE *getmntf(dnp)
   struct DiskNode *dnp;
{
   NODE *np;
   NODE **ch;
   int i;
   
   if ((np = gspgnode()) == NULL) return(np); /* no frames */
   np->prepcode = unpreparednode;
   Memcpy(np->cda, dnp->cda, sizeof(CDA));
   (np->cda)[0] &= 0x7f;  /* clear node flag in cda */
   np->flags = dnp->flags & DNGRATIS;
#if NFGRATIS != DNGRATIS
#error fix above assignment
#endif
   Memcpy(&(np->allocationid), dnp->allocationid, 4);
   Memcpy(&(np->callid),       dnp->callid,       4);
   np->flags |= NFALLOCATIONIDUSED | NFCALLIDUSED;
   for (i=0; i<16; i++) {  /* copy the keys */
      switch ((np->keys)[i].type = (dnp->keys)[i].ik.keytype) {
       case datakey:
       case misckey:
       case chargesetkey:
       case devicekey:
       case copykey:
         Memcpy((np->keys)[i].nontypedata.dk11.databody11,
                (dnp->keys)[i].dkdk.databody11, 11);
         break;
       case pagekey:
       case segmentkey:
       case nodekey:
       case meterkey:
       case fetchkey:
       case startkey:
       case resumekey:
       case domainkey:
       case sensekey:
       case frontendkey:
         (np->keys)[i].databyte =
           (dnp->keys)[i].ik.databyte;
         Memcpy((np->keys)[i].nontypedata.ik.item.upk.cda,
                (dnp->keys)[i].ik.cda, sizeof(CDA));
         Memcpy(&(np->keys)[i].nontypedata.ik.item.upk.allocationid,
                (dnp->keys)[i].ik.allocationid, 4);
         break;
       case nrangekey:
       case prangekey:
         Memcpy((np->keys)[i].nontypedata.rangekey.rangecda,
                (dnp->keys)[i].rangekey.cda, sizeof(CDA));
         Memcpy((np->keys)[i].nontypedata.rangekey.rangesize,
                (dnp->keys)[i].rangekey.rangesize, 5);
         break;
       default: crash("GET992 Unrecognized key type in disk node");
      }
   }
   /* put into hash chain */
   ch = &(anodechainheads[cdahash(np->cda) & nodechainhashmask]);
   np->hashnext = *ch;
   *ch = np;
   np->preplock = HILRU;
   return(np);
}
 
 
/*********************************************************************
findnodeinpot - Find a CDA in a node pot
 
  Input -
     cda - Address of the CDA of the node to copy (high (=node) bit on)
     cte - Pointer to the coretable entry for the nodepot
 
  Output -
      Returns pointer to the disk node (crashes if not found)
*********************************************************************/
static struct DiskNode *findnodeinpot(const uchar *cda, CTE *cte)
{
   struct NodePot *np =
         (struct NodePot*)map_window(IOSYSWINDOW, cte, MAP_WINDOW_RO);
   int i;
   for (i=0; i<NPNODECOUNT; i++) {
      if (!Memcmp(np->disknodes[i].cda, cda, sizeof(CDA)))
         return np->disknodes+i;
   }
   crash("GETC002 - findnodeinpot could not find CDA in pot");
} /* End findnodeinpot */
 
 
/*********************************************************************
movenodetoframe - Move a CDA from a disknode to a node frame
 
  Input -
     cda - Address of the CDA of the node to copy (high (=node) bit on)
     cte - Pointer to the coretable entry for the nodepot
 
  Output -
      Returns NULL if no node frames available
      Otherwise returns pointer to the node frame allocated
  Caller must call checkforcleanstart.
*********************************************************************/
NODE *movenodetoframe(const uchar *cda, CTE *cte)
{
   NODE *n;
 
   corelock_page(cte);      /* Ensure node pot stays over gspgnode */
   n = getmntf(findnodeinpot(cda, cte));
   coreunlock_page(68, cte);
   return(n);
} /* End movenodetoframe */
 
 
/*********************************************************************
setupcteandpage - Make user page available to other kernel routines
 
  Input -
     pcfa - Pointer to the cda, flags, and allocation ID for page
     cte - Pointer to the coretable entry for the page
 
  Output -
      Returns kernel address of the page (mapped into IOSYSWINDOW)
*********************************************************************/
uchar *setupcteandpage(PCFA *pcfa, CTE *cte)
{
   register CTE **cp;
   uchar *p = map_window(IOSYSWINDOW, cte, MAP_WINDOW_RW);
 
   Memcpy(cte->use.page.cda, pcfa->cda, sizeof(CDA));
   cte->use.page.allocationid = pcfa->allocationid;
 
   /* The following assertions are necessary for the following code */
   /* to correctly set the flags field in the core table entry */
 
#if adatacheckread != ctcheckread
#error adatacheckread and ctcheckread must be the same bit
#endif
#if adatavirtualzero != ctvirtualzero
#error adatavirtualzero and ctvirtualzero must be the same bit
#endif
#if adatagratis != ctgratis
#error adatagratis and ctgratis must be the same bit
#endif
 
   cte->flags &= ~(ctgratis|ctcheckread);
   cte->flags |= ctallocationidused |
                 (pcfa->flags & (ctcheckread|ctgratis));
 
   cte->ctefmt = PageFrame;        /* Mark as page */
   cte->use.page.leftchain = cte->use.page.rightchain =
         (union Item *)cte;
   cp = &(apagechainheads[cdahash(cte->use.page.cda)
                          & pagechainhashmask]);
   cte->hashnext = *cp;         /* Place at head of hash chain */
   *cp = cte;
   return (p);
} /* End setupcteandpage */
 
 
/*********************************************************************
setupvirtualzeropage - Materialize a virtual zero page in a frame
 
  Input -
     pcfa - Pointer to the cda, flags, and allocation ID for page
     cte - Pointer to the coretable entry for the page
         cte->type == FreeFrame
 
  Output - None
*********************************************************************/
void setupvirtualzeropage(PCFA *pcfa, CTE *cte)
{
   register CTE **cp;
 
   if (cte->ctefmt != FreeFrame)
      crash("GET567 suvzp of non FreeFrame");
   cte->ctefmt = PageFrame;
   cte->use.page.leftchain = cte->use.page.rightchain =
      (union Item *)cte;
   Memcpy(cte->use.page.cda, pcfa->cda, sizeof(CDA));
   cte->use.page.allocationid = pcfa->allocationid;
 
   /* The following assertions are necessary for the following code */
   /* to correctly set the flags field in the core table entry */
 
#if adatacheckread != ctcheckread
#error adatacheckread and ctcheckread must be the same bit
#endif
#if adatavirtualzero != ctvirtualzero
#error adatavirtualzero and ctvirtualzero must be the same bit
#endif
#if adatagratis != ctgratis
#error adatagratis and ctgratis must be the same bit
#endif
 
   cte->flags &= ~(ctbackupversion|ctgratis|ctcheckread);
   cte->flags |= ctallocationidused |
                 (pcfa->flags & (ctcheckread|ctgratis));
 
   cp = &(apagechainheads[cdahash(cte->use.page.cda)
                          & pagechainhashmask]);
   cte->hashnext = *cp;         /* Place at head of hash chain */
   *cp = cte;

   if(!cte->zero) clear_page(cte);
   cte->flags &=~ctchanged; /* we didn't change it, we
        only initialized it. */
} /* End setupvirtualzeropage */
 
 
/*********************************************************************
getsubvp - Set up backup version of a page
 
  Input -
     cte - Pointer to the coretable entry for the page
         cte->extensionflags & ctwhichbackup = !0 for backup version
                                             = 0 for next backup
 
  Output - None
*********************************************************************/
void getsubvp(CTE *cte)
{
   register CTE *c;
   int bit = (currentswaparea != SWAPAREA1 ? ctwhichbackup : 0);
 
   cte->extensionflags ^= bit;  /* Set ctwhichbackup relative to */
                                  /* the currentswaparea */
   if (cte->extensionflags & bit) c = &buppage2;
   else c = &buppage1;    /* Get correct chain head */
   cte->use.page.rightchain = (union Item*)c;
   cte->use.page.leftchain = c->use.page.leftchain;
   (c->use.page.leftchain)->cte.use.page.rightchain = (union Item*)cte;
   c->use.page.leftchain = (union Item*)cte;
} /* End getsubvp */
 
 
/*********************************************************************
setupversion - Set up version from a request (GETENDSU in assembler)
 
  Input -
     cte - Pointer to the coretable entry for the page
     req - Pointer to the request
 
  Output -
     0  - Page has been discarded
     1  - Page is set up
*********************************************************************/
int setupversion(CTE *cte, REQUEST *req)
{
   DEVREQ *drq = req->devreqs;
   if (!drq) crash("GETC003 No devreqs completed");
   if (drq->devreq) crash("GET004 Devreq not last one");
 
   switch (gdiverrq(drq)) {
    case gdiverrq_current:
      cte->flags &= ~ctbackupversion;  /* Current version */
      setupcteandpage(&req->pcfa, cte);
      break;
    case gdiverrq_backup:
      cte->flags |= ctbackupversion;   /* Backup version */
      cte->extensionflags |= ctwhichbackup; /* make next backup */
      setupcteandpage(&req->pcfa, cte);
      getsubvp(cte);
      break;
    case gdiverrq_neither:
      getredrq(drq);
      gspmpfa(cte);      /* Discard the page */
      return 0;
   }
   getredrq(drq);
   return 1;
} /* End setupversion */
 
 
/*********************************************************************
getunlok - Unlock a lock from the outstanding I/O bit array and run
           any waiters.
 
  Input -
     id  - hash(id) is the outstanding I/O bit array index
 
  Output - None
*********************************************************************/
void getunlok(uint32 id)
{
   uint32 off = id & OUTSTANDINGIOMASK;  /* Compute hash */
   uint32 mask = (unsigned long)0x80000000 >> (off & 31);
 
   if ( (outstandingio[off>>5] ^= mask) & mask)
      crash("GETC005 - Outstanding I/O bit already off");
   enqmvcpu(ioqueues+(id & IOQUEUESMASK));
} /* End getunlok */
 
 
/*********************************************************************
getenqio - Enqueue a domain on an I/O queue
 
  Input -
     id  - hash(id) is the I/O queue index
     dom - Pointer to the domain root or NULL
 
  Output - None
*********************************************************************/
void getenqio(uint32 id, NODE *dom)
{
   if (dom) {
      enqueuedom(dom, ioqueues+(id & IOQUEUESMASK));
   }
} /* End getenqio */
 
 
/*********************************************************************
getlock - Acquire a bit in the outstanding I/O array
 
  Input -
     id  - hash(id) is the outstanding I/O array bit index
     dom - Pointer to the domain root to enqueue or NULL
 
  Output -
     0 - Bit is already in use, domain enqueued on associated I/O queue
     1 - Bit is obtained, domain not enqueued on associated I/O queue
*********************************************************************/
int getlock(uint32 id, NODE *dom)
{
   uint32 off = id & OUTSTANDINGIOMASK;  /* Compute hash */
   uint32 mask = (unsigned long)0x80000000 >> (off & 31);
 
   if (outstandingio[off>>5] & mask) {
      outstandingioclash++;      /* Count the conflict */
      getenqio(id, dom);
      return 0;
   }
   outstandingio[off>>5] |= mask;
   return 1;
} /* End getlock */
 
 
/*********************************************************************
getendedcleanup - Clean up after a "get" type I/O operation completes
 
  Input -
     req - Pointer to the request that completed
 
  Output -
     Number of devreqs that completed successfully
*********************************************************************/
int getendedcleanup(REQUEST *req)
{
   int count = 0;
   DEVREQ **drqp;            /* Pointer to pointer to current devreq */
 
   if (req->pcfa.flags & REQPOT)    /* a pot */
      getunlok(pothash(req->potaddress)); /* Release the lock */
   else {                           /* a page */
      uint32 id = cdahash(req->pcfa.cda);
      if (req->pcfa.flags & REQHOME)   /* A read from home */
         getunlok(id);                    /* Release the lock */
      else                             /* A swap area page */
         enqmvcpu(ioqueues+(id & IOQUEUESMASK)); /* just run queue */
   }
    /* See if a DEVREQ completed successfully. */
   for (drqp = &req->devreqs; *drqp; ) {  /* Count and save good one */
      if (DEVREQCOMPLETE == (*drqp)->status) {
         if (count++) crash("GETC006 More than one devreq complete");
         drqp = &(*drqp)->devreq;  /* Keep good one on chain */
      } else {                  /* Bad or not executed */
         DEVREQ *drq = *drqp;      /* save bad/unexecuted devreq */
         *drqp = (*drqp)->devreq;  /* Dechain it */
         getredrq(drq);            /* and free it */
      }
   }
   if (!count) free_any_page(req);
   return count;
} /* End getendedcleanup */
 
 
/*********************************************************************
getended - Post processing for a "get" type I/O operation
 
  Input -
     req - Pointer to the request that completed
 
  Output - None
*********************************************************************/
void getended(REQUEST *req)
{
   if (getendedcleanup(req) == 1) {  /* I/O successful */
      CTE *cte = req->pagecte;
      if (cte->ctefmt != InTransitFrame)
         crash("GET449 getended frame not InTransit");
      if (req->pcfa.flags & REQPOT) {  /* This is a pot */
         if (!(req->pcfa.flags & REQALLOCATIONPOT)) { /* Node pot */
            if (req->potaddress.range < 0) { /* swap pot */
               cte->use.pot.nodepottype = nodepottypeswapclean;
               numbernodepotsincore[nodepottypeswapclean]++;
            } else {
               cte->use.pot.nodepottype = nodepottypehome;
               numbernodepotsincore[nodepottypehome]++;
            }
            setupnodepot(cte, req);
         } else {                      /* allocation pot */
            numberallocationpotsincore++;
            setuppots(cte, &allocationpotchainhead, req);
            cte->ctefmt = AlocPotFrame;
         }
         if (!(cte->extensionflags & ctkernellock))
            crash("GETC007 CTE not kernel locked on call to getended");
         cte->extensionflags &= ~ctkernellock;  /* Unlock cte */
      } else {                /* Page read */
         if (setupversion(cte, req)) {  /* page has been set up */
            cte->flags |= ctreferenced;
            if (!(cte->extensionflags & ctkernellock))
               crash("GETC008 Page CTE not kernel locked ");
            cte->extensionflags &= ~ctkernellock;  /* Unlock cte */
         }
      }
   }
   getrereq(req);
} /* End getended */
 
 
/*********************************************************************
getreqenqueue - Enqueue an I/O operation
 
  Input -
     req        - REQUEST block for the operation
     actor      - Domain root of the actor
 
  Output - None
        io_noioreqblocks
*********************************************************************/
static void getreqenqueue(REQUEST *req,
                          void (*endingproc)(REQUEST *req), NODE *actor)
{
   uint32 hashin;
 
   req->doneproc = endingproc;    /* Procedure to call when done */
   req->completioncount = 1;      /* Need only one to complete */
   if (req->pcfa.flags & REQPOT)  /* if request for a pot */
      hashin = pothash(req->potaddress);
   else hashin = cdahash(req->pcfa.cda);
   putdomainifanyonqueue(actor, ioqueues + (hashin & IOQUEUESMASK));
   gddenq(req);
   logicalpageio++;        /* Each request is one logical page I/O */
} /* End getreqenqueue */
 
 
/*********************************************************************
getreqnm - Read a node for migration
 
  Input -
     cda        - Pointer to CDA of the node
     type       - REQUEST type (for use if I/O needed)
     endingproc - Procedure to call when I/O is finished
     actor      - Domain root of the actor
 
  Output -
     a struct CodeIOret describing the result - Returns:
        io_notmounted
        io_notreadable
        io_potincore            ioret is pointer to CTE for pot
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
        io_notindirectory       CDA not in requested directory(s)
*********************************************************************/
struct CodeIOret getreqnm(CDA cda, int type,
                          void (*endingproc)(REQUEST *req), NODE *actor)
{
   struct CodeIOret ret;
 
   switch ( (ret = gdiblook(cda, actor)).code ) {
    case io_notmounted:     /* ret is value to return */
    break;
    case io_notreadable:    /* ret is value to return */
    break;
    case io_potincore:      /* ret is value to return */
    break;
    case io_pagezero:       /* ret is value to return */
      crash("GETC009 getreqnm got io_pagezero from gdiblook");
      ret.code = io_notreadable;  /* If continue, say not readable */
    break;
    case io_cdalocked:      /* ret is value to return */
    break;
    case io_noioreqblocks:  /* ret is value to return */
      putdomainifanyonqueue(actor, &noiorequestblocksqueue);
    break;
    case io_notindirectory: /* ret is value to return */
    break;
    case io_built: {
         REQUEST *req = ret.ioret.request;
 
         req->type = type;
         req->pagecte = NULL;
         getreqenqueue(req, endingproc, actor);
         ret.code = io_started;
      }
    break;
    default: crash("GETC010 Unexpected return code from gdiblook");
      ret.code = io_notreadable;  /* If continue, say not readable */
   }
   return ret;
} /* End getreqnm */
 
 
/*********************************************************************
getreqreadhome - Read from the home area
 
  Input -
     req        - Pointer to the REQUEST for the operation
     endingproc - Procedure to call when I/O is finished
     id         - hash(id) is the outstanding I/O array bit index
     actor      - Domain root of the actor
 
  Output -
     a struct CodeIOret describing the result - Returns:
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
*********************************************************************/
static
struct CodeIOret getreqreadhome(
   REQUEST *req,
   void (*endingproc)(REQUEST *req),
   uint32 id,
   NODE *actor,
   struct GRTReadInfo loc)
{
   struct CodeIOret ret;
 
   req->pcfa.flags |= REQHOME;
   if (!getlock(id, actor)) {
      getrereq(req);             /* Return the request block */
      ret.code = io_cdalocked;
      return ret;
   }
   for ( ; loc.device; ) {
      DEVREQ *devreq = acquiredevreq(req);
 
      if (!devreq) {
         getendedcleanup(req);
         getrereq(req);             /* Return the request block */
         putdomainifanyonqueue(actor, &noiorequestblocksqueue);
         ret.code = io_noioreqblocks;
         return ret;
      }
      devreq->device = loc.device;
      devreq->offset = loc.offset;
      md_dskdevreqaddr(devreq);
      loc = grtnext();
   }
   getreqenqueue(req, endingproc, actor);
   ret.code = io_started;
   return ret;
} /* End getreqreadhome */
 
 
/*********************************************************************
getreqhp - Read a home node pot
 
  Input -
     cda        - Pointer to CDA of the node
     type       - REQUEST type (for use if I/O needed)
     endingproc - Procedure to call when I/O is finished
     actor      - Domain root of the actor
 
  Output -
     a struct CodeIOret describing the result - Returns:
        io_notmounted
        io_notreadable          ioret is rangeloc of node pot
        io_potincore            ioret is pointer to CTE for pot
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
*********************************************************************/
struct CodeIOret getreqhp(CDA cda, int type,
                          void (*endingproc)(REQUEST *req), NODE *actor)
{
   struct CodeGRTRet ret;
   struct CodeIOret ourret;
 
   ret = grthomen(cda);
   switch (ret.code) {
    case grt_notmounted:
       ourret.code = io_notmounted;
    break;
    case grt_notreadable:
       ourret.code = io_notreadable;
       ourret.ioret.rangeloc = ret.ioret.rangeloc;
    break;
    case grt_potincore:
       ourret.code = io_potincore;
       ourret.ioret.cte = ret.ioret.cte;
    break;
    case grt_mustread: {
         REQUEST *req = acquirerequest();
 
         if (!req) {
            putdomainifanyonqueue(actor, &noiorequestblocksqueue);
            ourret.code = io_noioreqblocks;
    break;
         }
         req->pcfa.flags = REQPOT;  /* Clear REQALLOCATIONPOT */
/* readhomepot */
         req->potaddress = ret.ioret.readinfo.id.potaddress;
         req->type = type;
         req->pagecte = NULL;
         ourret = getreqreadhome(req,
                                 endingproc,
                                 pothash(req->potaddress),
                                 actor,
                                 ret.ioret.readinfo);
      }
    break;
    default: crash("GETC011 Unexpected return code from grthomen");
      ret.code = io_notreadable;  /* If continue, say not readable */
   }
   return ourret;
} /* End getreqhp */
 
 
/*********************************************************************
getreqn - Read node
 
  Input -
     cda        - Pointer to CDA of the node
     type       - REQUEST type (for use if I/O needed)
     endingproc - Procedure to call when I/O is finished
     actor      - Domain root of the actor
 
  Output -
     a struct CodeIOret describing the result - Returns:
        io_notmounted
        io_notreadable
        io_potincore            ioret is pointer to CTE for pot
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
*********************************************************************/
struct CodeIOret getreqn(CDA cda, int type,
                         void (*endingproc)(REQUEST *req), NODE *actor)
{
   struct CodeIOret ret;
 
   switch ( (ret = gdilook(cda, actor)).code ) {
    case io_notmounted:     /* ret is value to return */
    break;
    case io_notreadable:    /* ret is value to return */
    break;
    case io_potincore:      /* ret is value to return */
    break;
    case io_cdalocked:      /* ret is value to return */
    break;
    case io_noioreqblocks:  /* ret is value to return */
      putdomainifanyonqueue(actor, &noiorequestblocksqueue);
    break;
    case io_notindirectory: /* ret is value to return */
       ret = getreqhp(cda, type, endingproc, actor);
    break;
    case io_built: {
         REQUEST *req = ret.ioret.request;
 
         req->type = type;
         req->pagecte = NULL;
         getreqenqueue(req, endingproc, actor);
         ret.code = io_started;
      }
    break;
    default: crash("GETC012 Unexpected return code from gdilook");
      ret.code = io_notreadable;  /* If continue, say not readable */
   }
   return ret;
} /* End getreqn */
 
 
/*********************************************************************
getreqbn - Read backup version of a node
 
  Input -
     cda        - Pointer to CDA of the node
     type       - REQUEST type (for use if I/O needed)
     endingproc - Procedure to call when I/O is finished
     actor      - Domain root of the actor
 
  Output -
     a struct CodeIOret describing the result - Returns:
        io_notmounted
        io_notreadable
        io_potincore            ioret is pointer to CTE for pot
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
*********************************************************************/
static struct CodeIOret getreqbn(CDA cda, int type,
                         void (*endingproc)(REQUEST *req), NODE *actor)
{
   struct CodeIOret ret;
 
   switch ( (ret = gdilbv(cda, actor)).code ) {
    case io_notmounted:     /* ret is value to return */
    break;
    case io_notreadable:    /* ret is value to return */
    break;
    case io_potincore:      /* ret is value to return */
    break;
    case io_cdalocked:      /* ret is value to return */
    break;
    case io_noioreqblocks:  /* ret is value to return */
      putdomainifanyonqueue(actor, &noiorequestblocksqueue);
    break;
    case io_notindirectory: /* ret is value to return */
       ret = getreqhp(cda, type, endingproc, actor);
    break;
    case io_built: {
         REQUEST *req = ret.ioret.request;
 
         req->type = type;
         req->pagecte = NULL;
         getreqenqueue(req, endingproc, actor);
         ret.code = io_started;
      }
    break;
    default: crash("GETC013 Unexpected return code from gdilbv");
      ret.code = io_notreadable;  /* If continue, say not readable */
   }
   return ret;
} /* End getreqbn */
 
 
/*********************************************************************
getreqap - Read an allocation pot
 
  Input -
     rl         - RANGELOC for the pot
     type       - REQUEST type (for use if I/O needed)
     endingproc - Procedure to call when I/O is finished
                  (it must call getended)
     actor      - Domain root of the actor or NULL
 
  Output -
     a struct CodeIOret describing the result - Returns:
        io_notmounted
        io_notreadable
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
*********************************************************************/
struct CodeIOret getreqap(RANGELOC rl, int type,
                         void (*endingproc)(REQUEST *req), NODE *actor)
{
   struct CodeGRTRet ret;
   struct CodeIOret ourret;
 
   switch ( (ret = grtrrr(rl)).code ) {
    case grt_notmounted:
       ourret.code = io_notmounted;
    break;
    case grt_notreadable:
       ourret.code = io_notreadable;
    break;
    case grt_mustread: {
         REQUEST *req = acquirerequest();
 
         if (!req) {
            putdomainifanyonqueue(actor, &noiorequestblocksqueue);
            ourret.code = io_noioreqblocks;
    break;
         }
         req->pcfa.flags = REQPOT | REQALLOCATIONPOT;
         req->potaddress = rl;
         req->type = type;
         req->pagecte = NULL;
         ourret = getreqreadhome(req,
                              endingproc,
                              pothash(req->potaddress),
                              actor,
                              ret.ioret.readinfo);
      }
    break;
    default: crash("GETC014 Unexpected return code from grtrrr");
      ret.code = io_notreadable;  /* If continue, say not readable */
   }
   return ourret;
} /* End getreqap */
 
 
/*********************************************************************
getreqpcommon - Common code for reading current and backup pages
 
  Input -
     ret        - Information returned from directory lookup
     cda        - Pointer to CDA of the page (not already in storage)
     type       - REQUEST type (for use if I/O needed)
     endingproc - Procedure to call when I/O is finished
     actor      - Domain root of the actor or NULL
 
  Output - Ret has been update for ending condition, codes are:
        io_notmounted
        io_notreadable
        io_pagezero             ioret is *PCFA for virtual zero page
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
*********************************************************************/
void getreqpcommon(
   struct CodeIOret *ret,
   const CDA cda,
   int type,
   void (*endingproc)(REQUEST *req),
   NODE *actor,
   CTE *cte) /* CTE to read into, or NULL to allocate one */
{
   struct CodeGRTRet grtret;
 
   switch (ret->code ) {
    case io_notmounted:     /* ret is value to return */
    break;
    case io_notreadable:    /* ret is value to return */
    break;
    case io_pagezero:
       localpcfa = *ret->ioret.pcfa;  /* Save pcfa of page */
    break;
    case io_cdalocked:      /* ret is value to return */
    break;
    case io_noioreqblocks:  /* ret is value to return */
      putdomainifanyonqueue(actor, &noiorequestblocksqueue);
    break;
    case io_built: {
         REQUEST *req = ret->ioret.request;
 
         req->type = type;
         req->pagecte = cte;
         getreqenqueue(req, endingproc, actor);
         ret->code = io_started;
      }
    break;
    case io_notindirectory: /* ret is value to return */
      grtret = grthomep(cda);
      switch (grtret.code) {
       case grt_notmounted:
          ret->code = io_notmounted;
       break;
       case grt_notreadable:
          ret->code = io_notreadable;
          ret->ioret.rangeloc = grtret.ioret.rangeloc;
       break;
       case grt_readallopot:
          *ret = getreqap(grtret.ioret.rangeloc,
                          type, endingproc, actor);
       break;
       case grt_mustread:
         if (grtret.ioret.readinfo.id.pcfa->flags & adatavirtualzero) {
            localpcfa = *grtret.ioret.readinfo.id.pcfa;
                   /* Save pcfa of page */
            ret->ioret.pcfa = &localpcfa; /* avoid confusion */
            ret->code = io_pagezero;
         } else {
            REQUEST *req = acquirerequest();
 
            if (!req) {
               putdomainifanyonqueue(actor, &noiorequestblocksqueue);
               ret->code = io_noioreqblocks;
       break;
            }
            req->pcfa = *grtret.ioret.readinfo.id.pcfa;
            req->pcfa.flags &= REQCHECKREAD | REQGRATIS;
            req->type = type;
            req->pagecte = cte;
            *ret = getreqreadhome(req,
                                  endingproc,
                                  cdahash(req->pcfa.cda),
                                  actor,
                                  grtret.ioret.readinfo);
         }
       break;
       default:
         crash("GETC015 Unexpected return code from grthomep");
         ret->code = io_notreadable;
      }
    break;
    default: crash("GETC016 Unexpected CodeIOret.code");
      ret->code = io_notreadable;  /* If continue, say not readable */
   }
} /* End getreqpcommon */
 
 
/*********************************************************************
getreqp - Read page
 
  Input -
     cda        - Pointer to CDA of the page (not already in storage)
     type       - REQUEST type (for use if I/O needed)
     endingproc - Procedure to call when I/O is finished
     actor      - Domain root of the actor or NULL
 
  Output -
     a struct CodeIOret describing the result - Returns:
        io_notmounted
        io_notreadable
        io_pagezero             ioret is *PCFA for virtual zero page
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
*********************************************************************/
struct CodeIOret getreqp(
   const CDA cda,
   int type,
   void (*endingproc)(REQUEST *req),
   NODE *actor)
{
   struct CodeIOret ret;
 
   ret = gdilook(cda, actor);
   getreqpcommon(&ret, cda, type, endingproc, actor, NULL);
   return ret;
} /* End getreqp */
 
 
/*********************************************************************
getreqpm - Read a page for migration
 
  Input -
     cda        - Pointer to CDA of the page, must not be virtual zero
     type       - REQUEST type (for use if I/O needed)
     endingproc - Procedure to call when I/O is finished
                  (it must call getended)
     actor      - Domain root of the actor or NULL
 
  Output -
     a struct CodeIOret describing the result - Returns:
        io_notmounted
        io_notreadable
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
        io_notindirectory
*********************************************************************/
struct CodeIOret getreqpm(CDA cda, int type,
                         void (*endingproc)(REQUEST *req), NODE *actor)
{
   struct CodeIOret ret;
 
   ret = gdiblook(cda, actor);
   getreqpcommon(&ret, cda, type, endingproc, actor, NULL);
   return ret;
} /* End getreqpm */
 
 
/*********************************************************************
getreqbp - Read backup version of a page
 
  Input -
     cda        - Pointer to CDA of the page (not already in storage)
     type       - REQUEST type (for use if I/O needed)
     endingproc - Procedure to call when I/O is finished
     actor      - Domain root of the actor or NULL
 
  Output -
     a struct CodeIOret describing the result - Returns:
        io_notmounted
        io_notreadable
        io_pagezero             ioret is *PCFA for virtual zero page
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
*********************************************************************/
static struct CodeIOret getreqbp(CDA cda, int type,
                         void (*endingproc)(REQUEST *req), NODE *actor)
{
   struct CodeIOret ret;
 
   ret = gdilbv(cda, actor);
   getreqpcommon(&ret, cda, type, endingproc, actor, NULL);
   return ret;
} /* End getreqbp */
 
 
/*********************************************************************
getreqba - Read allocation data for the backup version of a page
 
  Input -
     cda        - Pointer to the CDA of the page
     type       - REQUEST type (for use if I/O needed)
     endingproc - Procedure to call when I/O is finished
                  (it must call getended)
     actor      - Domain root of the actor or NULL
 
  Output -
     a struct CodeIOret describing the result - Returns:
        io_notmounted
        io_notreadable
        io_started
        io_cdalocked            CDA may already be in transit
        io_noioreqblocks
        io_allocationdata
*********************************************************************/
struct CodeIOret getreqba(CDA cda, int type,
                         void (*endingproc)(REQUEST *req), NODE *actor)
{
   struct CodeIOret ret;
 
   if (ret.ioret.pcfa)
      ret.code = io_allocationdata;
   else {
      RANGELOC rl;
 
      rl = grtcrl(cda);
      if (rl.range == -1 && rl.offset == USHRT_MAX) {
         ret.code = io_notmounted;
      } else {
         switch ( (ret = grtfadfp(rl, cda)).code ) {
          case io_allocationdata: /* ret is value to return */
          break;
          case io_readpot:
            rl = ret.ioret.rangeloc;
            ret = getreqap(rl, type, endingproc, actor);
          break;
          default:
            crash("GETC017 Unexpected return code from grtfadfp");
            ret.code = io_notreadable;
         }
      }
   }
   return ret;
} /* End getreqba */
 
 
/*********************************************************************
handlegetreqreturn - Handle the returned data from getreqn or getreqp
 
  Input -
     ret           - The returned data from above routines
     localpfca.cda - A copy of the CDA with the high bit on for a node
 
  Output -
        get_wait         - Actor has been queued
        get_ioerror      - I/O error reading node
        get_tryagain     - Look again for the node or page
*********************************************************************/
static int handlegetreqreturn(struct CodeIOret *ret, const CDA cda)
{
 
   switch (ret->code) {
    case io_notmounted:
      enqueuedom(cpuactor, &rangeunavailablequeue);
      checkforcleanstart();
      return get_wait;
    case io_notreadable:
/*...*/crash("handlegetreqreturn returning get_ioerror");
      checkforcleanstart();
      return get_ioerror;
    case io_potincore: {
         CTE *cte = ret->ioret.cte;
         int i;
         NODE *nf = movenodetoframe(cda, cte);
         if (!nf) {
            enqueuedom(cpuactor, &nonodesqueue);
            checkforcleanstart();
            return get_wait;
         }
           /* Count nodes fetched from pot by type of pot */
         i = cte->use.pot.nodepottype;
         if (i>4) crash("GETC018 nodepottype out of range");
         else nodeinpot[i]++;
         corelock_node(3, nf);     /* Keep node over checkforcleanstart */
         checkforcleanstart();
         coreunlock_node(nf);
         if (iosystemflags & DISPATCHINGDOMAINSINHIBITED) {
            enqueuedom(cpuactor, &frozencpuqueue);
            return get_wait;
         }
         return get_tryagain;
      }
    case io_pagezero:       /* ioret is *PCFA for virtual zero page */
      localpcfa.flags = ret->ioret.pcfa->flags;  /* Save PCFA fields */
      localpcfa.allocationid = ret->ioret.pcfa->allocationid;
      {  CTE *cte = gspgpage();
         if (!cte) {
            enqueuedom(cpuactor, &nopagesqueue);
            checkforcleanstart();
            return get_wait;
         }
         setupvirtualzeropage(&localpcfa, cte);
         cte->flags |= ctreferenced;  /* Give it a good lru */
         corelock_page(cte);     /* Keep it over checkforcleanstart */
         checkforcleanstart();
         coreunlock_page(66, cte);
      }
      return get_tryagain;
    case io_started:
      if (localpcfa.cda[0] & 0x80) nodepotfetches++;
      /* Fall thru into not started cases */
    case io_cdalocked:      /* CDA may already be in transit */
    case io_noioreqblocks:
      checkforcleanstart();
      return get_wait;
    default: crash("GET019 getreqp returned bad code");
   }
} /* End handlegetreqreturn */
 
 
/*********************************************************************
getpage - Cause a page to exist in main storage
 
  Input -
     cda        - Pointer to the CDA of the page
 
  Output -
        get_wait         - Actor has been queued
        get_ioerror      - I/O error reading page
        get_tryagain     - Look again for the page
*********************************************************************/
int getpage(const CDA cda)
{
   struct CodeIOret ret;
 
if(lowcoreflags.getlogenable){char str[80];
 sprintf(str, "getpage cda=%x %x\n", (int)b2long(cda, 2), (int)b2long(cda+2,4));
 logstr(str);/*...*/}
   if (!(cpuactor->preplock & 0x80))
      crash("GETC020 Getpage called with unlocked cpuactor");
   Memcpy(localpcfa.cda, cda, sizeof(CDA));
   ret = getreqp(cda, REQNORMALPAGEREAD, getended, cpuactor);
   return handlegetreqreturn(&ret, cda);
} /* End getpage */
 
 
/*********************************************************************
getnode - Cause a node to exist in node space
 
  Input -
     cda        - Pointer to the CDA of the node
 
  Output -
        get_wait         - Actor has been queued
        get_ioerror      - I/O error reading node
        get_tryagain     - Look again for the node
*********************************************************************/
int getnode(CDA cda)
{
   struct CodeIOret ret;
 
if(lowcoreflags.getlogenable){char str[80];
 sprintf(str, "getnode cda=%x %x\n", (int)b2long(cda, 2), (int)b2long(cda+2,4));
 logstr(str);/*...*/}
   if (!(cpuactor->preplock & 0x80))
      crash("GETC021 Getpage called with unlocked cpuactor");
   Memcpy(localpcfa.cda, cda, sizeof(CDA));
   localpcfa.cda[0] |= 0x80;        /* Set NODE bit in cda */
   ret = getreqn(localpcfa.cda, REQNORMALPAGEREAD, getended, cpuactor);
   return handlegetreqreturn(&ret, localpcfa.cda);
} /* End getnode */
 
 
/*********************************************************************
getbvp - Cause the backup version of a page to be read
 
  Input -
     cda        - Pointer to the CDA of the node
     cpuactor must hold actor to queue
  Output - Codes returned in the codepcfa
        get_wait         - Actor has been queued
        get_ioerror      - I/O error reading page
        get_virtualzero  - Page is virtual zero, pcfa for page returned
*********************************************************************/
struct codepcfa getbvp(CDA cda)
{
   struct CodeIOret ret;
   struct codepcfa ourret;
 
   ret = getreqbp(cda, REQNORMALPAGEREAD, getended, cpuactor);
   switch (ret.code) {
    case io_notmounted:
      checkforcleanstart();
      ourret.code = get_notmounted;
      break;
    case io_pagezero:
      localpcfa = *ret.ioret.pcfa;
      ourret.pcfa = &localpcfa;
      ourret.code = get_virtualzero;
      break;
    default:
      ourret.code = handlegetreqreturn(&ret, cda);
      break;
   }
   return ourret;
} /* End getbvp */
 
 
/*********************************************************************
getalid - Get flags and allocation data for a page
 
  Input -
     cda        - Pointer to the CDA of the node
     cpuactor must hold actor to queue
  Output - Codes returned in the codepcfa
        get_wait         - Actor has been queued
        get_ioerror      - I/O error reading node
        get_gotpcfa      - pcfa for the page returned
                           flags have checkread, virtualzero, and gratis
*********************************************************************/
struct codepcfa getalid(CDA cda)
{
   struct CodeGRTRet grtret;
   struct codepcfa ourret;
 
   ourret.pcfa = gdiladnb(cda);      /* Look in working directory */
   if (ourret.pcfa) {
      ourret.code = get_gotpcfa;
      return ourret;
   }
   ourret.pcfa = gdiladbv(cda);      /* Look in backup directory */
   if (ourret.pcfa) {
      ourret.code = get_gotpcfa;
      return ourret;
   }
     /* Not in a directory, must get if from an allocation pot */
   switch ( (grtret = grthomep(cda)).code ) {
    case grt_notmounted:
      enqueuedom(cpuactor, &rangeunavailablequeue);
      checkforcleanstart();
      ourret.code = get_wait;
      break;
    case grt_notreadable:
      checkforcleanstart();
      ourret.code = get_ioerror;
      break;
    case grt_readallopot:
      switch (getreqap(grtret.ioret.rangeloc,
                     REQNORMALPAGEREAD, getended, cpuactor).code) {
       case io_notmounted:
         enqueuedom(cpuactor, &rangeunavailablequeue);
         checkforcleanstart();
         ourret.code = get_wait;
         break;
       case io_notreadable:
         checkforcleanstart();
         ourret.code = get_ioerror;
         break;
       case io_started:
         /* Fall thru into not started */
       case io_cdalocked:
       case io_noioreqblocks:
         checkforcleanstart();
         ourret.code = get_wait;
         break;
       default:
         crash("GETC022 unexpected return code from grtreqap");
      }
      break;
    case grt_mustread:
      ourret.pcfa = grtret.ioret.readinfo.id.pcfa;
      ourret.code = get_gotpcfa;
      break;
    default:
      crash("GETC023 Unexpected return code from grthomep");
      ourret.code = get_ioerror;
   }
   return ourret;
} /* End getalid */
 
 
/*********************************************************************
getbvn - Cause the backup version of a node to be read
 
  Input -
     cda        - Pointer to the CDA of the node
     cpuactor must hold actor to queue
  Output - Codes returned in the codepcfa
        get_wait         - Actor has been queued
        get_ioerror      - I/O error reading page
        get_gotdisknode  - Pointer to the disk node returned.
        get_notmounted   - Node's disk(s) is not mounted
*********************************************************************/
struct CodeDiskNode getbvn(CDA cda)
{
   struct CodeIOret ret;
   struct CodeDiskNode ourret;
 
   ret = getreqbn(cda, REQNORMALPAGEREAD, getended, cpuactor);
   switch (ret.code) {
    case io_notmounted:
      checkforcleanstart();
      ourret.code = get_notmounted;
      break;
    case io_potincore:
      ourret.disknode = findnodeinpot(cda, ret.ioret.cte);
      ourret.code = get_gotdisknode;
      break;
    default:
      ourret.code = handlegetreqreturn(&ret, cda);
      break;
   }
   return ourret;
} /* End getbvn */
