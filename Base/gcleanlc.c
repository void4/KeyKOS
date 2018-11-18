/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/*  GCLEANLC - Code to maintain the clean list - KeyTech Disk I/O */

#include <string.h>
#include <limits.h>
#include "lli.h"
#include "sysdefs.h"
#include "kerinith.h"
#include "keyh.h"
#include "locksh.h"
#include "spaceh.h"
#include "memomdh.h"
#include "queuesh.h"
#include "wsh.h"
#include "kschedh.h"
#include "memoryh.h"
#include "devmdh.h"
#include "ioreqsh.h"
#include "ioworkh.h"
#include "dskiomdh.h"
#include "getih.h"
#include "geteh.h"
#include "get2gclh.h"
#include "gcleanlh.h"
#include "gcl2gswh.h"
#include "gckpth.h"
#include "gdirecth.h"
#include "gmi2gclh.h"
#include "grt2gclh.h"
#include "guinth.h"
#include "sparc_mem.h"
#include "memutil.h"


/* Variable shared with gswapac.c */

bool multiswapdevices;


#define STARTCLEANTHRESHOLD 3
#define NUMBEROFCLEANLISTENTRIES 6
 
typedef struct CleanListEntry CLE;
struct CleanListEntry {
   CLE *next, *prev;            /* Doublely linked list pointers */
   CTE *page;                   /* CTE Pointer to page to be cleaned */
   int writecount;              /* Number of copies left to write */
   DEVICE *writtenon;           /* Device already written on */
};
 
 
/* Local static variables */
 
static
int cleanlistfrozen = 0;            /* Flag for no more adds */
static
int cleanlistentries = 0;           /* Number of entries on the list */
static
CLE cleanlist =                     /* The clean list header */
     {&cleanlist, &cleanlist, NULL, 0, NULL};
static
CTE *cleancursor;                   /* Next CTE to check for cleaning */
static
CLE *cleanlistfreelist = NULL;      /* Available entries */
 
static
CLE cleanlistblocks[NUMBEROFCLEANLISTENTRIES]; /* Free entries */
 
 
 
/*********************************************************************
icleanl - Initialize the clean list module
 
  Input - None
 
  Output - None
*********************************************************************/
void icleanl(void)
{
   int i;
 
   for (i = 0; i < NUMBEROFCLEANLISTENTRIES; i++) {
      cleanlistblocks[i].next = cleanlistfreelist;
      cleanlistfreelist = cleanlistblocks + i;
   }
   cleancursor = firstcte;
} /* End icleanl */
 
 
/*********************************************************************
pageisallzeroes - Test for and handle a page which is all zeroes
 
  Input -
     cte    - Pointer to the Core Table Entry for the page to be tested
 
  Output -
     1 if page is all zeroes and is cleaned, else 0
     If 1 returned, called must check continuecheckpoint.
*********************************************************************/
static int pageisallzeroes(CTE *cte)
{
   int i = pagesize / sizeof(uint32);
   unsigned char savedflags = cte->flags; /* save ctreferenced */
   uint32 *p = (uint32 *) map_window(QUICKWINDOW, cte, MAP_WINDOW_RO);
   PCFA pcfa;
 
   for ( ; i--; p++) {           /* Scan page for non-zeroes */
      if (*p) {
         cte->flags = savedflags;   /* restore ctreferenced from scan */
         nonzerobytesscanned += (pagesize - sizeof(uint32)
                                - i * sizeof(uint32));
         nonzeropages++;
         return 0;
      }
   }
   /* Page is all zeroes */
   /* Clean as VZ page */
   cte->flags = savedflags;   /* restore ctreferenced from scan */
   mark_page_clean(cte);
   Memcpy(pcfa.cda, cte->use.page.cda, sizeof(CDA));
   pcfa.flags = cte->flags;
   pcfa.allocationid = cte->use.page.allocationid;
   gdisetvz(&pcfa);                       /* Record VZ in directory */
   virtualzeropages++;
   if (cte->flags & ctkernelreadonly) gckdecpc(cte);
      return 1;
} /* End pageisallzeroes */
 
 
/*********************************************************************
formatpageforcleanlist - Get a clean list entry and perform basic
                         formating
 
  Input -
     cte    - Pointer to the Core Table Entry for the page to be cleaned
 
  Output -
     Pointer to the Clean List Entry or NULL if none are available
*********************************************************************/
static CLE *formatpageforcleanlist(CTE *cte)
{
   CLE *cle = cleanlistfreelist;
   int i;
 
   if (!cle) {
      nocleanlistentries++;
      return NULL;
   }
   cleanlistfreelist = cle->next;
   cleanlistentries++;
 
   cle->page = cte;               /* Entry for this page */
   cle->writtenon = NULL;         /* Written on no devices */
   switch (cte->ctefmt) {
    case PageFrame:
      gdiclear(cte->use.page.cda);
      cle->writecount = (grtnplex(cte->use.page.cda) ? 2 : 1);


      if (!multiswapdevices) cle->writecount = 1;  /* KLUGE for Alan */


      break;
    case NodePotFrame:
      cle->writecount = 2;
      break;
    default: 
      crash("GCLEANLC001 add for non-page/nodepot");
      break;
   }
   if (!cte->iocount) cte->extensionflags |= ctmarkcleanok;
   cte->iocount = (i = cte->iocount + cle->writecount);
   if (i > CHAR_MAX) crash("GCLEANLC002 iocount overflow");
   cte->extensionflags |= ctoncleanlist;
   return cle;
} /* End formatpageforcleanlist */
 
 
/*********************************************************************
insertfirstincleanlist - Put an entry at the head of the list
 
  Input -
     cle    - Pointer to the Clean List Entry to insert
 
  Output - None
*********************************************************************/
static void insertfirstincleanlist(CLE *cle)
{
   cle->next = cleanlist.next;
   cle->prev = &cleanlist;
   cleanlist.next->prev = cle;
   cleanlist.next = cle;
} /* End insertfirstincleanlist */
 
 
/*********************************************************************
insertlastincleanlist - Put an entry at the head of the list
 
  Input -
     cle    - Pointer to the Clean List Entry to insert
 
  Output - None
*********************************************************************/
static void insertlastincleanlist(CLE *cle)
{
   cle->prev = cleanlist.prev;
   cle->next = &cleanlist;
   cleanlist.prev->next = cle;
   cleanlist.prev = cle;
} /* End insertlastincleanlist */
 
 
/*********************************************************************
returncleanlist - Return a clean list entry to the free pool
 
  Input -
     cle    - Pointer to the Clean List Entry to insert
 
  Output - None
*********************************************************************/
static void returncleanlist(CLE *cle)
{
   cle->page->extensionflags &= ~ctoncleanlist; /* Unmark page */
   cle->prev->next = cle->next;  /* Dequeue from list */
   cle->next->prev = cle->prev;
   cle->next = cleanlistfreelist;
   cleanlistfreelist = cle;
   if (!(cleanlistentries--))
      crash("GCLEANL004 Cleanlistentries count underflow");
} /* End returncleanlist */
 
 
/*********************************************************************
addpageornodepottocleanlist - Add a page or a nodepot to the clean
                              list during checkpoint mode processing
 
  Input - None
 
  Output -
     Pointer to the added clean list entry or NULL
     Caller must check continuecheckpoint.
*********************************************************************/
static CLE *addpageornodepottocleanlist(void)
{
   CTE *cte = cleancursor;
 
   do {
      if   (!(cte->extensionflags & ctoncleanlist)
           && !cte->iocount
           && (cte->flags & ctkernelreadonly)) {
         if (!pageisallzeroes(cte)) {
            CLE *cle = formatpageforcleanlist(cte);
            if (cle) insertlastincleanlist(cle);
            cleancursor = cte;
            return cle;
         }
      }
      if (++cte == lastcte) cte = firstcte;
   } while (cte != cleancursor);
   return NULL;             /* No new pages added */
} /* End addpageornodepottocleanlist */
 
 
/*********************************************************************
stillwanttocleanit - Check if page should not be cleaned
 
  Input -
     cle    - Pointer to the clean list entry to test
 
  Output -
     1 if entry still should be cleaned. 0 if cle should be freed
*********************************************************************/
static int stillwanttocleanit(CLE *cle)
{
   CTE *cte = cle->page;
 
   switch (cte->ctefmt) {
    case NodePotFrame:
      if  (!cte->use.pot.nodepotnodecounter
            && (!cle->writtenon
                || !(--cte->iocount))) {
         if (cte->flags & ctkernelreadonly) gckdecpc(cte);
         gspmpfa(cte);
         return 0;
      }
      break;
    case PageFrame:
      if (!(iosystemflags & (CHECKPOINTMODE | WAITFORCLEAN))) {
         /* Not in a special mode, check for recent reference */
         if   (cte->flags & ctreferenced  /* Referenced since added */
               && !cle->writtenon) {      /* and not written anywhere */
            cte->iocount -= cle->writecount;
            return 0;
         }
      }
      break;
    default:
      crash("GCLEANLC006 Non-page, non-nodepot found on clean list");
   }
   return 1;
} /* End stillwanttocleanit */
 
 
/*********************************************************************
gcladd - Add a page or nodepot to the clean list
 
  Input -
     cte    - Pointer to the Core Table Entry to add
 
  Output -
     1 if cleaning should be started, 0 if not yet necessary
*********************************************************************/
int gcladd(CTE *cte)
{
   if (!cleanlistfrozen && !(cte->extensionflags & ctoncleanlist)) {
      if (!pageisallzeroes(cte)) {
         CLE *cle = formatpageforcleanlist(cte);
         if (cle) insertlastincleanlist(cle);
      }
   }
   if (cleanlistentries < STARTCLEANTHRESHOLD) return 0;
   return 1;
} /* End gcladd */
 
 
/*********************************************************************
gclctpn - Clean This Page Now
 
  Input -
     cte    - Pointer to the Core Table Entry to add
 
  Output -
     1 if page was all zeroes and is now clean, 0 otherwise
*********************************************************************/
int gclctpn(CTE *cte)
{
   if (!cleanlistfrozen) {
      if (cte->extensionflags & ctoncleanlist) {
         CLE *cle;
 
         for (cle = cleanlist.next;
              cle != &cleanlist && cle->page != cte;
              cle = cle->next) { ; }
         if (cle == &cleanlist)
            crash("GCLEANLC003 cte marked on cleanlist but not found");
         cle->prev->next = cle->next;  /* Dequeue from list */
         cle->next->prev = cle->prev;
         insertfirstincleanlist(cle);
      } else {            /* Not on list - Build new entry */
         if (pageisallzeroes(cte)) return 1;
         {
            CLE *cle = formatpageforcleanlist(cte);
            if (cle)
               insertfirstincleanlist(cle);
         }
      }
   }
   return 0;
} /* End gclctpn */
 
 
/*********************************************************************
gclfreze - Prevent or allow new additions to the clean list
 
  Input -
     p      - Non-zero to freeze the list, zero to thaw it
 
  Output - None
*********************************************************************/
void gclfreze(int p)
{
   cleanlistfrozen = p;
} /* End gclfreze */
 
 
/*********************************************************************
gcleanmf - Clean me first
 
  Input -
     old    - Pointer to CTE for kernel read only page to clean quickly
 
  Output -
     cte of copy of page if it could be copied (the current version)
         otherwise NULL
*********************************************************************/
CTE *gcleanmf(CTE *old)
{
   PCFA pcfa;
   CTE *new, *returncte;
   uchar *np, *op;            /* For copying page's data */
 
   if (old->hashnext == old)
      crash("GCLEANL005 Core table entry hash chained to self");
   if (!(old->flags & ctkernelreadonly))
      crash("GCLEANL006 Core table entry not kernel read only");
   if (PageFrame != old->ctefmt)
      crash("GCLEANL007 gcleanmf called with nonpage");
   if ((old->corelock > HILRU  /* Must not move current version */
        || old->devicelockcount)   /* Must not move current version */
       && old->iocount) {       /* must not move next backup version either */
         return NULL;
   }
   corelock_page(old);


#ifdef NOCOPYKRO
   new = NULL;
#else  NOCOPYKRO
   new = gspgpage();
#endif NOCOPYKRO


   coreunlock_page(63, old);
   if (!new) {
      if  (!(old->extensionflags & ctoncleanlist)
           && !old->iocount
           && !(old->flags & ctchanged)) { /* N.B. This test assumes
                   that no map entry for a KRO page has the changed
                   bit on; ctchanged tells it all. */
                   /* e.g. clean write error sets ctchanged */
         CLE *cle = formatpageforcleanlist(old);
         if (cle) insertfirstincleanlist(cle);
      }
      return NULL;             /* Return - Not copied */
   }
   if ((old->corelock > HILRU  /* Must not move current version */
        || old->devicelockcount)   /* Must not move current version */
       && old->iocount) {       /* must not move next backup version either */
         gspmpfa(new);          /* Free frame for copy */
         return NULL;
   }
   op = map_window(QUICKWINDOW, old, MAP_WINDOW_RO);
   np = map_window(CLEANWINDOW1, new, MAP_WINDOW_RW);
   Memcpy (np, op, pagesize);
 
   /* Page at "new" now holds same data as page at "old" */
   /*    Copy selected coretable entry fields */
 
   new->flags = old->flags;
   Memcpy(pcfa.cda, old->use.page.cda, sizeof(CDA));
   pcfa.flags = old->flags & (ctgratis | ctcheckread);
   pcfa.allocationid = old->use.page.allocationid;
   new->corelock = HILRU;          /* Unlock copy */
   if (old->iocount) {             /* Cleaning old page */
                         /* Old page will be next backup version, KRO*/
                         /* New page will be current version*/
      new->flags &= ~ctkernelreadonly;
      new->iocount = 0;
      gspdetpg(old);
      setupcteandpage(&pcfa, new);
      new->flags |= ctchanged;
      returncte = new;
   } else {            /* OK to move next backup version */
                                /* Old page will be current version */
                       /* New page will be next backup version, KRO */
      resetkro(old);       /* New page is already marked KRO */
      returncte = old;
      old = new;
   }
   /* "old" is the CTE of the backup version of the page */
 
   if (!(old->flags & ctkernelreadonly))
      crash("GCLEANLC045 backup version not kernel read only");
   old->flags |= ctbackupversion;
   old->extensionflags &= ~ctwhichbackup;
   setupcteandpage(&pcfa, old);
   getsubvp(old);
   old->flags |= ctchanged;
   old->corelock = (old->corelock & ~HILRU) /* Clean it fast */
                         | 2;               /* but not too fast */
   return returncte;
} /* End gcleanmf */
 
 
/*********************************************************************
gclbuild - Select pages to clean to a particular device
 
  Input -
     dev    - Pointer to the device to clean onto
 
  Output -
     Not zero - One or more pages selected. Zero - No pages selected
     In either case, "pagecleanneeded" is cleared if no further
     cleaning is required.
*********************************************************************/
int gclbuild(DEVICE *dev)
{
   int numberbuilt = 0;
   CLE *cle = cleanlist.next;
 
   gccicln();                /* Start new list to clean */
   for ( ;numberbuilt < MAXPAGESTOCLEANATATIME; ) {
      if (&cleanlist == cle) {
         if (iosystemflags & CHECKPOINTMODE) {
            cle = addpageornodepottocleanlist();
            if (!cle)
   break;
         }
         else break;
      }
 
      /* "cle" now points to a clean list entry to consider */
 
      if (!stillwanttocleanit(cle)) {
         CLE *tofree = cle;            /* Just remove from list */
 
         cle = cle->next;
         returncleanlist(tofree);
      } else {               /* Do continue to clean this page */
 
         /* Check if already cleaned on requested device */
 
         if    (dev != cle->writtenon
                || !multiswapdevices) {  /* Write it on this device */
            CTE *cte = cle->page;
 
            if (gccbncs(dev, cte)) {     /* Added to io list */
               numberbuilt++;            /* Indicate built one */
               if (cte->extensionflags & ctmarkcleanok) {
                  cte->extensionflags &= ~ctmarkcleanok;
                  switch (cte->ctefmt) { /* Update write check data if needed */
                   case NodePotFrame:
                     gmiintnp(cte);
                     cte->flags &= ~ctchanged; /* Reset changed bit */
                     break;
                   case PageFrame:
                     if (cte->flags & ctcheckread) guintcr(cte);
                     mark_page_clean(cte);
                     break;
                   default: crash("GCL176 bad ctefmt on clean list");
                  }
               }
               if (!cle->writtenon) {       /* 1st write */
                  logicalpageio++;
                  cle->writtenon = dev;
               }
               if (!(--cle->writecount)) {  /* Written enough copies */
                  CLE *tofree = cle;
                  cle = cle->next;
                  returncleanlist(tofree);
               } else cle = cle->next;     /* Write more */
            } else                 /* Out of space in io list */
   break;                          /* Stop trying */
         } else cle = cle->next;   /* Already on this dev, try next */
      }
   }
   if  ( (iosystemflags & (CHECKPOINTMODE | WAITFORCLEAN)
          && cleanlistentries == 0)
        || cleanlistentries > STARTCLEANTHRESHOLD)
      iosystemflags &= ~PAGECLEANNEEDED;
   return numberbuilt;
} /* End gclbuild */
