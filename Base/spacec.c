/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "lli.h"
#include "sysdefs.h"
#include "kktypes.h"
#include "cvt.h"
#include "keyh.h"
#include "disknodh.h"
#include "wsh.h"
#include "locksh.h"
#include "memomdh.h"
#include "spaceh.h"
#include "ioworkh.h"
#if !defined(diskless_kernel)
#include "key2dskh.h"
#include "gdi2spah.h"
#include "gckpth.h"
#include "gcleanlh.h"
#endif
#include "unprndh.h"
#include "queuesh.h"
#include "getih.h"
#include "cvt.h"
#include "prepkeyh.h"
#include "scafoldh.h"
#include "kerinith.h"
#include "diskless.h"
#include "memutil.h"

#define targetfreepages 5 /* number of pages to free at once */
 
NODE *freenodes = NULL;  /* list of free node frames */
CTE  *freepages = NULL;  /* list of free page frames */
unsigned int nfreepages; /* number of free page frames */
static unsigned int forsakensegtabthreshhold;
static unsigned int forsakenpagtabthreshhold;
 
void gspmnfa(np)
/* Make node frame available */
NODE *np;      /* ptr to node frame to free */
{
   np->leftchain = (np->rightchain = (union Item *)np);
   memzero(np->cda,6);  /* set cda to zero */
   np->flags = 0;   /* clear NFDIRTY */
   np->hashnext = freenodes;
   freenodes = np;
}
 
void gspmpfa(
/* Make page frame available */
CTE *cte)      /* ptr to page frame to free */
{
   if (cte->flags & ctkernelreadonly) crash("GSP001 - page is KRO");
   switch (cte->ctefmt) {
    case AlocPotFrame:
      numberallocationpotsincore--;
      break;
    case NodePotFrame:
      {
         unsigned int type = cte->use.pot.nodepottype;
         if (type > 4) crash("GSP100 nodepottype out of range");
         numbernodepotsincore[type]--;
      }
      break;
    case FreeFrame:
      break;
    default: break;
   }
   cte->ctefmt = FreeFrame;
   cte->flags = 0;
   cte->iocount = 0;
   cte->extensionflags &= ~ctkernellock;
   cte->corelock = 0;
   cte->hashnext = freepages;
   freepages = cte;
   nfreepages += 1;
}

static void unchain_hash(
/* Unchain a page frame from its hash chain. */
   CTE **hashhead, /* Pointer to hash chain head */
   CTE *cte)       /* The page frame to unchain */
{
   for (;;) {   /* First search the chain for it. */
      if (*hashhead == NULL)
         crash("SPACE124 page or pot not in its hash chain");
      if (*hashhead == cte) break; /* found our page */
      hashhead = &((*hashhead)->hashnext);
   }
   *hashhead = cte->hashnext; /* Unchain. */
}

void gspdetpg(CTE *cte)
/* Remove page frame from hash chain and from domains. */
{
   if (cte->ctefmt != PageFrame)
      crash("SPACE125 gspdetpg of non-page");
   if (cte->flags & ctbackupversion) {
      /* Remove from chain of backup versions. */
      register union Item *l = cte->use.page.leftchain,
                          *r = cte->use.page.rightchain;
      r->item.leftchain = l;
      l->item.rightchain = r;
   } else { /* not a backup version */
      union Item *k, *next;
      detpage(cte);
      /* Unprepare all keys to the page. */
      for (k = cte->use.page.leftchain;
           k != (union Item *)cte;
           k = next) {
         next = k->key.nontypedata.ik.item.pk.leftchain;
         k->key.nontypedata.ik.item.upk.allocationid
               = cte->use.page.allocationid;
         Memcpy(k->key.nontypedata.ik.item.upk.cda,
                cte->use.page.cda, sizeof(CDA));
         k->key.type &= ~prepared;
      }
   }
   /* Remove the page from its hash chain. */
   unchain_hash(&apagechainheads[cdahash(cte->use.page.cda)
                                 & pagechainhashmask],
                cte);
   /* At this point ctefmt is PageFrame, page is ready to be
      freed or reused. */
}

static CTE *corelruptr;  /* cursor for the clock algorithm */

CTE *gspgpage(void)
/* GSPGPAGE - Get an empty page frame
   Output -
      Returns NULL if no page frames available
      Otherwise a page frame with cte->type == FreeFrame
   Caller must call checkforcleanstart.
*/
{
   unsigned int loopcount;
   CTE *startingcursor;
tryfreelist:
   if (freepages != NULL) { /* there is a free page */
      CTE *thispage = freepages;
      freepages = thispage->hashnext;
      thispage->use.page.maps = 0;
      nfreepages -= 1;
      pageframesallocated++;
      return(thispage);
   }
 
   /* We need to find some pages to free. */

   /* No CPUs run domains here, so bits remain updated. */
   loopcount = HILRU+1; /* set max times through search loop */
   startingcursor = corelruptr;
   for (;;) {
      if (corelruptr->corelock & 0xf8) goto corelrunext;
      switch (corelruptr->ctefmt) {
       case PageFrame:
       case NodePotFrame:
       case AlocPotFrame:
         if (corelruptr->iocount) {
            if (!(iosystemflags & WAITFORCLEAN)) goto corelrunext;
            /* we are waiting to clean this page. */
            iosystemflags |= PAGECLEANNEEDED;
   goto exitloop;  /* Don't continue, lest we steal a much more recently used
              page that happens to be clean. */
         }
         else if (corelruptr->flags & ctreferenced) {
            if (PageFrame == corelruptr->ctefmt)
               mark_page_unreferenced(corelruptr);
            else corelruptr->flags &= ~ctreferenced;
            corelruptr->corelock = HILRU;
            goto corelrunext;
         }
#if defined(diskless_kernel)
         else goto corelrunext; /* no place to clean pages */
#else /* !diskless_kernel */
         else if (corelruptr->flags & ctchanged) {
            if (corelruptr->corelock != 0
                && (corelruptr->corelock -= 1) > 1) goto corelrunext;
            /* Lru value (after decrementing) is 0 or 1. */
            if (corelruptr->extensionflags & ctkernellock) goto corelrunext;
            /* Try to clean this page. */
            if (corelruptr->corelock == 0) {
               /* We tried once already. Try harder. */
               if (gclctpn(corelruptr))
                  goto checkfreecount; /* we did clean it */
               iosystemflags |= PAGECLEANNEEDED | WAITFORCLEAN;
   goto exitloop;   /* Don't continue, lest we steal a much more recently used
                  page that happens to be clean. */
            }
            if (gcladd(corelruptr)) iosystemflags |= PAGECLEANNEEDED;
            goto corelrunext;
         } else { /* not referenced, not changed */
            if (corelruptr->corelock != 0
                && (corelruptr->corelock -= 1) != 0) goto corelrunext;
            /* This page is least recently used. */
            if (corelruptr->extensionflags & ctkernellock
                || corelruptr->devicelockcount != 0) goto corelrunext;
            /* Free the page. */
            switch (corelruptr->ctefmt) {
             case PageFrame:
               gspdetpg(corelruptr);
               break;
             case AlocPotFrame:
               unchain_hash(&allocationpotchainhead, corelruptr);
               break;
             case NodePotFrame:
               unchain_hash(&apagechainheads[
                     pothash(corelruptr->use.pot.potaddress)
                                    & pagechainhashmask],
                            corelruptr);
               break;
            }
            iosystemflags &= ~WAITFORCLEAN;
            gspmpfa(corelruptr);
            goto checkfreecount;
         }
#endif /* diskless_kernel */
         crash ("Spooky control flow.");
           /* Shouldn't get here. Bug in 88K code if we do! */
checkfreecount:
         if (nfreepages >= targetfreepages)
   goto exitloop;  /* Got enough free pages for now. Return one. */
          break;
        default:    /* Other types aren't stealable */
          break;
      } /* end of switch on ctefmt */
corelrunext:
      if (++corelruptr == apageend) { /* wrap around */
         corelruptr = firstcte;
         pagewraps++;  /* keep a statistic */
      }
      if (corelruptr == startingcursor) {
         /* We went all the way around. */
         if (--loopcount == 0)
   break;   /* Worst case loop termination */
      }
   } /* end of loop through core table */
exitloop:
   if (freepages != NULL) goto tryfreelist;
   return NULL;
}

unsigned long nodesmarkedforcleaning;
     /* The number of nodes with NFNEEDSCLEANING on. */

static NODE *nodecleancursor; /* Where to start cleaning */
#if !defined(diskless_kernel)
bool gspcleannodes(void)
/* Clean Nodes. */
/* Builds a node pot out of nodes marked for cleaning. */
/* Returns TRUE iff cleaned some. */
/* Caller must call checkforcleanstart. */
{
   CTE *cte;
   int count;
   struct NodePot *npot;
   struct DiskNode *dn;
   register NODE *localcursor;

   if (iosystemflags & INHIBITNODECLEAN) return FALSE;
   cte = gspgpage();  /* get a page frame for the node pot */
   if (cte == NULL) return FALSE;
   npot = (struct NodePot *)map_window(space_win, cte, MAP_WINDOW_RW);
   dn = npot->disknodes;
   cte->use.pot.nodepotnodecounter = 0;
   count = NPNODECOUNT;
   localcursor = nodecleancursor;
   for (;;) {
      if (localcursor->flags & NFNEEDSCLEANING) {
         if (localcursor->preplock > 1  /* Referenced since it was marked */
             && !(iosystemflags & CHECKPOINTMODE)) {
            localcursor->flags &= ~NFNEEDSCLEANING; /* forget it */
            nodesmarkedforcleaning--;
         } else if (localcursor->prepcode == unpreparednode
                    || unprnode(localcursor) == unprnode_unprepared) {
            /* Node definitely needs cleaning and is unprepared */
            int process = localcursor->domhookkey.type == pihk
                 && localcursor->domhookkey.databyte;
            /* Copy node to disknode. */
            Memcpy(dn->cda, localcursor->cda, sizeof(CDA));
            dn->cda[0] |= 0x80; /* set node bit */
            dn->flags = localcursor->flags & NFGRATIS;
                 /* NFGRATIS same as DNGRATIS */
            if (process) dn->flags |= DNPROCESS;
            long2b(localcursor->allocationid, dn->allocationid, 4);
            long2b(localcursor->callid, dn->callid, 4);
            {  /* Copy keys */
               int i;
               DISKKEY *dk = dn->keys;
               struct Key *k = localcursor->keys;
               for (i=16; i>0; i--)
                  key2dsk(k++, dk++);
            }
            localcursor->flags &= ~(NFDIRTY | NFNEEDSCLEANING);
            nodesmarkedforcleaning--;
            gdireset(dn->cda, cte, process);
            dn++;
            if (--count == 0)
		   break; /* node pot is full */
         }
      } /* end of if needs cleaning */
      /* Go on to next node frame. */
      if (++localcursor == anodeend)
         localcursor = firstnode;
      if (localcursor == nodecleancursor)
   break;    /* Scanned all the node frames. */
   } /* end of loop over nodes */
   nodecleancursor = localcursor; /* pick up here next time */
   if (count == NPNODECOUNT) {
      /* Didn't clean any nodes. */
      gspmpfa(cte);
      return FALSE;
   }
   /* Finish setting up the node pot. */
   cte->ctefmt = NodePotFrame;
   cte->use.pot.nodepottype = nodepottypeswapdirty;
   numbernodepotsincore[nodepottypeswapdirty]++;
#if !defined(diskless_kernel)
   if (iosystemflags & CHECKPOINTMODE)
      gckincpc(cte);
#endif
   /* At this point cte->use.pot.potaddress is undefined
      and the pot is not in the hash chains. */
   cte->flags &= ~ctreferenced; /* Just want to clean it. */
   cte->corelock = 2; /* The page will be cleaned when the LRU is
        decremented from 2 to 1. */
   enqmvcpu(&nonodesqueue);
   nodecleanneeded = FALSE;
   return TRUE;
}
#endif
 
static NODE *nodelruptr;
NODE *gspgnode(void)
/* GSPGNODE - Get an empty node frame
   Output -
      Returns NULL if no node frames available
      Else ptr to node frame, preplocked.
*/
/* Caller must call checkforcleanstart. */
{
   if (freenodes != NULL) {
      NODE *thisnode = freenodes;
      freenodes = freenodes->hashnext;
      nodeframesallocated++;
      return(thisnode);
   } else {
      NODE *startingcursor = nodelruptr;
      int loopcount = HILRU+1; /* max times through loop */
      for (;;) { /* Loop checking node LRU's */
noderelook:
         if ((nodelruptr->preplock & 0x80) == 0 /* not preplocked */
             && nodelruptr->corelock == 0){  /* and not corelocked */
            if (nodelruptr->flags & NFDIRTY) { /* node is dirty */
               switch (nodelruptr->preplock) { /* check LRU value */
                default: break;   /* > 1: leave it */
                case 0:
                  if (nodelruptr->flags & NFNEEDSCLEANING) {
                     /* Node is still marked for cleaning.
                        Try to clear the congestion. */
                     nodecleancursor = nodelruptr; /* clean this node first */
                     if (gspcleannodes()) /* if cleaned some */
                        goto noderelook; /* check it again
                                 (it could be dirty and recently used) */
                     else { /* could not clean any nodes */
                        nodecleanneeded = TRUE;
      return NULL; /* give up */
                     }
                  }
                  /* else must have been cleaned and dirtied again.
                     Fall into case below */
                case 1:
                  if (!(nodelruptr->flags & NFNEEDSCLEANING)) {
                     /* Mark node for cleaning. */
                     nodelruptr->flags |= NFNEEDSCLEANING;
                     if (++nodesmarkedforcleaning >= NPNODECOUNT) {
                        /* Marked a pot full. */
                        gspcleannodes(); /* Try to clean them */
                     }
                  }
                  break;
               }
            } else { /* node is clean */
               if (nodelruptr->preplock == 0 /* LRU is zero */
                   && !preplock_node(nodelruptr, lockedby_space)
                                       /* and we locked it */
                  ) {
#define NODELRUSMALLINT 3 /* max attempts to disentangle hooks */
                  unsigned int limit = NODELRUSMALLINT;
                     /* used to limit attempts to disentangle one node */
                  union Item *itm, *next;
                  /* Check all involved keys to this node. */
                  for (itm = nodelruptr->rightchain;
                       itm != (union Item *)nodelruptr  /* if a key */
                       && (itm->key.type & (involvedr | involvedw))
                                                   /* and involved */ ;
                       itm = next) {
                     next = itm->item.rightchain;
                     if (itm->key.type == pihk) {
                        /* Try to remove the hook. */
                        if (limit-- == 0  /* if this trip isn't worthwhile */
                            || !emptstal(&itm->key)) /* or it failed */
                           goto nodelruunlock; /* too bad */
                     }
                     /* Involved non-hooks will be uninvolved when we
                        unprepare the node. */
                  }
                  if (nodelruptr->domhookkey.type == pihk
                                            /* a hook in this node */
                      && !unhook(nodelruptr)) /* and we can't remove it */
                     goto nodelruunlock;  /* too bad */
                  switch (unprnode(nodelruptr)) {
                   case unprnode_cant:
                     goto nodelruunlock;
                   case unprnode_unprepared:
                     if (nodelruptr->flags & NFDIRTY) {
                        /* Unpreparing it dirtied it. */
                        unpreplock_node(nodelruptr);
                        goto noderelook; /* See if it should be cleaned. */
                     }
                     else goto nodelrugotnode; /* Steal this node. */
                  }
nodelruunlock:    unpreplock_node(nodelruptr); /* Can't use this after all */
               } /* end of LRU is zero and we locked */
            } /* end of node is clean */
            /* Decrement the node's LRU count */
            if (nodelruptr->preplock) nodelruptr->preplock--;
         } /* end not preplocked and not corelocked */
// nodelrunext:
         if (++nodelruptr == anodeend) { /* wrap around */
            nodelruptr = firstnode;
            nodewraps++;  /* keep a statistic */
         }
         if (nodelruptr == startingcursor) {
            /* We went all the way around. */
            if (--loopcount == 0)
      break;   /* Worst case loop termination */
         }
      } /* end of loop checking node LRU's */
      nodecleanneeded = TRUE;
      return NULL;  /* too bad */

nodelrugotnode:
      /* Steal the node at nodelruptr. */
      /* Unprepare all keys to the node. */
      {
         union Item *itm;
         for (itm = nodelruptr->rightchain;
              itm != (union Item *)nodelruptr;
              itm = nodelruptr->rightchain)
            unprepare_key(&itm->key);
      }
      {  /* Unprepare all keys in the node. */
         int i;
         struct Key *key = nodelruptr->keys;
         for (i=16; i>0; i--) {
            if (key->type & (involvedr|involvedw))
               crash("SPACE123 Key left involved");
            if (key->type & prepared) {
               /* Just unlink it. Slot will be scrapped. */
               register union Item *l = key->nontypedata.ik.item.pk.leftchain,
                                   *r = key->nontypedata.ik.item.pk.rightchain;
               l->item.rightchain = r;
               r->item.leftchain = l;
            }
            key++;
         }
      }
      {  /* Remove node from its hash chain. */
         NODE **p = &anodechainheads[cdahash(nodelruptr->cda)
                                     & nodechainhashmask];
         while (*p != nodelruptr)
            p = &((*p)->hashnext);
         *p = nodelruptr->hashnext;  /* unchain */
      }
      if (nodesmarkedforcleaning >= NPNODECOUNT)
         gspcleannodes();         /* I think this is unnecessary */
      nodeframesallocated++;
      return (nodelruptr);
   } /* end freenodes list empty */
}

void spaces()
/* Initialization. */
{
   forsakensegtabthreshhold =
      (apageend-firstcte) /* number of pages */  / 200;
   /* This ensures that no more than 0.5% of memory is taken up by
      forsaken segment tables, and no more than 0.5% by forsaken
      page tables. If there are more than that, we
      reclaim them. */
   if (!forsakensegtabthreshhold)
      forsakensegtabthreshhold = 1;  /* no less than one */
   /* This ensures that the kernel does not loop forever attempting to
      recover forsaken tables. */
   forsakenpagtabthreshhold = forsakensegtabthreshhold; /* same */
   corelruptr = firstcte;
   nodelruptr = firstnode;
   nodecleancursor = firstnode;
}

void 
hash_the_cda(CTE *cte)
{
	CTE **ch;

	cte->use.page.maps = 0;
	cte->use.page.allocationid = 1;
	/* Should consider doing something with plist[i].first */
	/* Should consider reading the data into a frame */
	cte->flags = ctallocationidused;
	/* put into hash chain */
	ch = apagechainheads
		+ (cdahash(cte->use.page.cda) & pagechainhashmask);
	cte->hashnext = *ch;
	*ch = cte;
}

