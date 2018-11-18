/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "keyh.h"
#include "cpujumph.h"
#include "wsh.h"
#include "geteh.h"
#include "getih.h"
#include "locksh.h"
#include "unprndh.h"
#include "prepkeyh.h"
#include "cvt.h"
#include "memutil.h"

void unprepare_key(
   struct Key *key)
/* Unprepare a prepared, uninvolved key. */
{
   union Item *l = key->nontypedata.ik.item.pk.leftchain,
              *r = key->nontypedata.ik.item.pk.rightchain;
   if (key->type & (involvedr | involvedw))
      crash("Unprepare involved key");
   l->item.rightchain = r;
   r->item.leftchain = l;
   if (key->type == prepared+pagekey) {
      CTE *cte = &(key->nontypedata.ik.item.pk.subject->cte);
      Memcpy(key->nontypedata.ik.item.upk.cda,
             cte->use.page.cda, sizeof(CDA));
      key->nontypedata.ik.item.upk.allocationid
            = cte->use.page.allocationid;
      cte->flags |= ctallocationidused;
   } else { /* a key to a node */
      NODE *np = &(key->nontypedata.ik.item.pk.subject->node);
      Memcpy(key->nontypedata.ik.item.upk.cda,
             np->cda, sizeof(CDA));
      if (key->type == resumekey+prepared) {
         key->nontypedata.ik.item.upk.allocationid
               = np->callid;
         np->flags |= NFCALLIDUSED;
      } else { /* not a resume key */
         key->nontypedata.ik.item.upk.allocationid
               = np->allocationid;
         np->flags |= NFALLOCATIONIDUSED;
      }
   }
   key->type &= ~prepared;
}
 
static int ensureprepnode(
   register NODE *node,    /* Pointer to the node */
   int code)             /* The required preparation code */
/* Ensure node is preparable as indicated or is unprepared. */
/*
   Output -
      Returns 1 If node is prepared wrong and can't be unprepared,
               or was already preplocked.
      Returns 0 If node was unprepared or prepared in correct mode.
               The node is now preplocked.
*/
// This routine is not called for prepcode = prepasstatenode.
// If it were ther would be a problem of ensuring that it was not shared.
{
   if (preplock_node(node,lockedby_ensureprepnode)) return 1;
   if (node->prepcode == code) return 0;
   if (node->prepcode == unpreparednode) return 0;
   if (unprnode(node) != unprnode_unprepared) return 1; /* couldn't unprepare */
   return 0;
}
 
 
NODE *srchnode(      /* Search for current version of node */
   register unsigned char *cda)  /* Pointer to the CDA to search for */
/*
   Output -
      Returns pointer to the node or NULL if node not in hash chains.
*/
{
   register NODE *n;
 
   for (n = anodechainheads[cdahash(cda) & nodechainhashmask];
        n != NULL && Memcmp(n->cda, cda, 6);
        n = n->hashnext)
      ;
   return n;
} /* End srchnode */
 
 
CTE *srchpage(        /* Search for current version of page */
   register const unsigned char *cda)  /* Pointer to the CDA to search for */
/*
   Output -
      Returns pointer to the cte or NULL if page not in hash chains.
*/
{
   register CTE *c;
 
   for (c = apagechainheads[cdahash(cda) & pagechainhashmask];
        c != NULL;
        c = c->hashnext)
      if (!Memcmp(c->use.page.cda, cda, 6)
          && PageFrame == c->ctefmt
          && !(c->flags & ctbackupversion) )
         break; /* found our page */
   return c;
} /* End srchpage */
 

CTE *srchbvop(
   register unsigned char *cda) /* Pointer to the CDA to search for */
/* Search for backup version of page */
/*
   Output -
      Returns pointer to the cte or NULL if page not in hash chains.
*/
{
   register CTE *c;
   register int bit = (currentswaparea == SWAPAREA1 ? ctwhichbackup : 0);

   for (c = apagechainheads[cdahash(cda) & pagechainhashmask];
        c != NULL;
        c = c->hashnext)
      if (!Memcmp(c->use.page.cda, cda, 6)
          && PageFrame == c->ctefmt
          && (c->flags & ctbackupversion)
          && (c->extensionflags & ctwhichbackup) == bit)
         break; /* found our page */
   return c;
} /* End srchbvop */

 
static int findnode(
   register struct Key *key)  /* Pointer to unprepared key to the node */
        /* Find the node designated by a key. Checks allocationid. */
/* Other input - cpuactor has actor */
/*
   Output -
*/
#define find_ioerror 0
               /* Permanent I/O error */
#define find_wait 1
               /* Actor enqueued on wait queue */
#define find_obsolete 2
               /* Key obsolete, changed to DK0 */
#define find_found 3
               /* key->subject set. Not chained, prepared bit not set */
{
   register NODE *n = srchnode(key->nontypedata.ik.item.upk.cda);
   if (n == NULL) {
#if defined(diskless_kernel)
	crash("JCV - Can't find a node!");
#else
      switch (getnode(key->nontypedata.ik.item.upk.cda)) {
       case get_ioerror:  return find_ioerror;
       case get_wait:     return find_wait;
       case get_tryagain: n=srchnode(key->nontypedata.ik.item.upk.cda);
      }
#endif
   }
   if (key->type == resumekey) {
      if (n->callid != key->nontypedata.ik.item.upk.allocationid) {
         *key = dk0;
         return find_obsolete;
      }
   }
   else {
      if (n->allocationid != key->nontypedata.ik.item.upk.allocationid){
         *key = dk0;
         return find_obsolete;
      }
   }
   key->nontypedata.ik.item.pk.subject = (union Item *)n;
   return find_found;
} /* End findnode */
 
 
static int findpage(
   register struct Key *key)  /* Pointer to unprepared key to page */
/* Find the page designated by a key. Checks allocationid. */
/* Other input - cpuactor has actor */
/*
   Output - Same as findnode, above
*/
{
   register CTE *p = srchpage(key->nontypedata.ik.item.upk.cda);
 
   if (p == NULL) {
#if defined(diskless_kernel)
	crash("JCV - Can't find a page!");
#else
      switch (getpage(key->nontypedata.ik.item.upk.cda)) {
       case get_ioerror:  return find_ioerror;
       case get_wait:     return find_wait;
       case get_tryagain: p=srchpage(key->nontypedata.ik.item.upk.cda);
      }
#endif
   }
   if (p->use.page.allocationid !=
                         key->nontypedata.ik.item.upk.allocationid){
     if(p->use.page.allocationid <
               key->nontypedata.ik.item.upk.allocationid)  
                     crash("Time warp page key!");
      *key = dk0;
      return find_obsolete;
   }
   key->nontypedata.ik.item.pk.subject = (union Item *)p;
   return find_found;
} /* End findpage */
 
 
void halfprep(key)         /* Chain a key maintaining midpointer etc. */
/* Input - */
register struct Key *key;     /* The key to be chained in */
/*
   The subject field of the key must point to the page or node and the
   prepared bit in the key type must be on.
*/
{
   register NODE *subject =
              (NODE *)key->nontypedata.ik.item.pk.subject;
   register union Item *k;
 
   if (key->type == resumekey+prepared) {
      if (subject->prepcode != prepasdomain) {
         for (k = subject->rightchain;
              k!=(union Item *)subject && k->key.type & (involvedr|involvedw);
              k = k->item.rightchain)
            ;
         k = k->item.leftchain;  /* back up one */
      }
      else k = subject->pf.dib->lastinvolved;
   }
   else k = subject->leftchain;
   key->nontypedata.ik.item.pk.leftchain = k;
   key->nontypedata.ik.item.pk.rightchain = k->item.rightchain;
   k->item.rightchain->item.leftchain = (union Item *)key;
   k->item.rightchain = (union Item *)key;
} /* End halfprep */
 
 
int prepkey(key)      /* Changes key to its prepared form. */
 /* Input: */
register struct Key *key;     /* Pointer to key to prepare */
 /* cpuactor has actor */
 /*
    Output - Returns int as follows: */
 /* prepkey_notobj - 0 - Obsolete key or does not designate page/node */
 /* prepkey_prepared - 1 - The key has been prepared */
 /* prepkey_wait - 2 - The key's object must be fetched, actor queued */
{
   register int rc;
 
   if (key->type & (prepared+involvedr+involvedw))
           crash ("PREPKEY002 prepkey - key already prepared/involved");
   switch (key->type) {
    case datakey:
    case misckey:
    case nrangekey:
    case prangekey:
    case chargesetkey:
    case devicekey:
    case copykey:
      return prepkey_notobj;
    case pagekey:
      rc = findpage(key);
      break;
    case segmentkey:
    case nodekey:
    case meterkey:
    case fetchkey:
    case startkey:
    case resumekey:
    case sensekey:
    case domainkey:
    case frontendkey:
      rc = findnode(key);
      break;
    case hookkey:
    default: crash ("PREPKEY003 bad key type to prepkey");
   }
   switch (rc) {
    case find_ioerror:  crash("PREPKEY004 Permanent I/O error on key");
    case find_wait:     return prepkey_wait;   /* Actor enqueued */
    case find_obsolete: return prepkey_notobj; /* Obsolete key */
    case find_found:
      key->type |= prepared; /* Subject field set */
      halfprep(key);
      return prepkey_prepared;
    default: crash ("PREPKEY005 Bad ruturn code from find(node/page)");
   }
} /* End prepkey */
 
 
void tryprep(key)        /* Prepare a key if possible without I/O. */
 /* Input: */
register struct Key *key;     /* Pointer to key to prepare */
/*
    Output - If key's object is in memory, then it has been prepared.
*/
{
   if (key->type & prepared)
           crash ("PREPKEY006 prepkey - key already prepared");
/*
   Test KEYTYPE to determine action
   If it is not a page or a node, just return
   Else see if the page or node is already in core
   If not in core, just return
*/
   switch (key->type & keytypemask) {
      register CTE *cte;              /* Incase key is a page key */
      register NODE *n;               /* Incase key is to a node */
    case datakey:
    case misckey:
    case nrangekey:
    case prangekey:
    case chargesetkey:
    case devicekey:
    case copykey:
      return;
    case pagekey:
      cte = srchpage(key->nontypedata.ik.item.upk.cda);
      if (cte == NULL) return;     /* Page is not in memory */
      if (cte->use.page.allocationid !=
                         key->nontypedata.ik.item.upk.allocationid){
         *key = dk0;
         return;                   /* Allocation id doesn't match */
      }
      key->nontypedata.ik.item.pk.subject = (union Item *)cte;
      break;
    case segmentkey:
    case nodekey:
    case meterkey:
    case fetchkey:
    case startkey:
    case resumekey:
    case sensekey:
    case domainkey:
    case frontendkey:
      n = srchnode(key->nontypedata.ik.item.upk.cda);
      if (n == NULL) return;
      if (key->type == resumekey) {
         if (n->callid != key->nontypedata.ik.item.upk.allocationid) {
            *key = dk0;
            return;
         }
      }
      else {
         if (n->allocationid !=
                    key->nontypedata.ik.item.upk.allocationid){
            *key = dk0;
            return;
         }
      }
      key->nontypedata.ik.item.pk.subject = (union Item *)n;
      break;
    default: crash ("PREPKEY007 bad key type to prepkey");
   }
   key->type |= prepared; /* Subject field set */
   halfprep(key);
   return;
} /* End tryprep */
 

void zaphook( /* Replace hook with involved DK(1) and turn off HOOKED. */
register struct Node *d) /* Pointer to the domain root
   with slot to clear */
{
struct Key *key = &d->domhookkey;
   register NODE *n = (NODE *)key->nontypedata.ik.item.pk.subject;
 
   if (key->type != pihk) crash("PREPKEY001 Zaphook of non-hook");
   if (d->prepcode == prepasdomain) d->pf.dib->readiness &= ~HOOKED;
   if ((n >= firstnode)  /* qh points to a node */
       && (n->prepcode == prepasdomain)  /* Which is a domain */
       && ((union Item *)key == n->pf.dib->lastinvolved)
       ) { /* Key is midptr */
      if (n->rightchain == n->pf.dib->lastinvolved) {
         n->flags &= ~NFREJECT;  /* only stallee, !reject */
      }
      n->pf.dib->lastinvolved = key->nontypedata.ik.item.pk.leftchain;
   }
   {  register union Item *left  = key->nontypedata.ik.item.pk.leftchain;
      register union Item *right = key->nontypedata.ik.item.pk.rightchain;
      left->item.rightchain = right;
      right->item.leftchain = left;
   }
   *key = dk1; key->type = datakey+involvedr+involvedw;
}
 
 
int involven(struct Key * key, int preptype)   /* Involves a key. */
/* Input:
      key - pointer to key to prepare, must designate a node, */
/*         can not be a resume key */
/* cpuactor has actor */

/*  preptype  - The prerequesite preperation code of the designated node */
/* Returns: */
   /* involven_ioerror - 0 - Permanent I/O error reading node */
   /* involven_wait - 1 - Actor enqueued for I/O */
   /* involven_obsolete - 2 - Key was obsolete, changed to dk0 */
   /* involven_preplocked - 3 - Designated node is
            differently prepared & can't be unprepared,
            or it was already preplocked. */
   /* involven_ok - 4 - Designated key prepared and involvedw.
            Designated node is now preplocked. */
{
   register NODE *subj;        /* The subject node for the key */
 
   if (key->type & prepared) {
      register NODE *left =       /* The left item for the key */
                 (NODE *)key->nontypedata.ik.item.pk.leftchain;
      subj = (NODE *)key->nontypedata.ik.item.pk.subject;
 
      if (ensureprepnode(subj, preptype))
         return involven_preplocked;
      if (left == subj ||
          ((struct Key *)left)->type & (involvedr+involvedw)) {
         /*
            If the subject is to the left of our key or the key to
            the left of our key is involved, then our key is already
            in the correct place in the prepared key chain. We only
            need to update the midpointer if the subject is prepared
            as a domain.
         */
         if (subj->prepcode == prepasdomain)
            subj->pf.dib->lastinvolved = (union Item *)key;
         key->type |= involvedw;
         return involven_ok;
      }
         /* Unchain the key from the backchain */
      left->rightchain = key->nontypedata.ik.item.pk.rightchain;
      ((NODE *)key->nontypedata.ik.item.pk.rightchain)->leftchain
               = (union Item *)left;
      subj = (NODE *)key->nontypedata.ik.item.pk.subject;
   }
   else {
      switch (findnode(key)) {
       case find_ioerror:
         return involven_ioerror;
       case find_wait:
         return involven_wait;
       case find_obsolete:
         return involven_obsolete;
       case find_found:
         break;
      }
 
      subj = (NODE *)key->nontypedata.ik.item.pk.subject;
      if (ensureprepnode(subj, preptype)) {
         key->type |= prepared;     /* Complete preparation of key */
         halfprep(key);
         return involven_preplocked;
      }
   } /* We now have subj pointing to the subject of a halfpreped key */
     /* Chain it into the involved posistion in the backchain */
   ((NODE *)key)->leftchain = (union Item *)subj;
   ((NODE *)key)->rightchain = subj->rightchain;
   ((NODE *)subj->rightchain)->leftchain = (union Item *)key;
   subj->rightchain = (union Item *)key;
   key->type |= prepared+involvedw;
   return involven_ok;
} /* End involven */
 
 
int involvep(key)       /* Involve a page key */
/* Input: */
register struct Key *key;
/*       - pointer to key to prepare, must designate a page */
/* cpuactor has actor */
/* Returns: */
 /*   involvep_ioerror - Permanent I/O error reading node */
 /*   involvep_wait - Actor enqueued for I/O */
 /*   involvep_obsolete - Key was obsolete, changed to dk0 */
 /*   involvep_ok - Key is now prepared + involvedw. */
{
   register CTE *subj;         /* The subject page for the key */
 
   if (key->type & prepared) { /* If the key is prepared */
      register CTE *left =        /* The left item for the key */
                 (CTE *)key->nontypedata.ik.item.pk.leftchain;
      register CTE *right =       /* The right item for the key */
                 (CTE *)key->nontypedata.ik.item.pk.rightchain;
 
         /* Unchain the key from the backchain */
      left->use.page.rightchain = (union Item *)right;
      right->use.page.leftchain = (union Item *)left;
   }
   else {
      switch (findpage(key)) {
       case find_ioerror:
         return involvep_ioerror;
       case find_wait:
         return involvep_wait;
       case find_obsolete:
         return involvep_obsolete;
       case find_found:
         break;
      }
   }
   subj = (CTE *)key->nontypedata.ik.item.pk.subject;
 
     /* We now have subj pointing to the subject of a halfpreped key */
     /* Chain it into the involved posistion in the backchain */
   key->nontypedata.ik.item.pk.leftchain = (union Item *)subj;
   key->nontypedata.ik.item.pk.rightchain = subj->use.page.rightchain;
   subj->use.page.rightchain->item.leftchain = (union Item *)key;
   subj->use.page.rightchain = (union Item *)key;
   key->type |= prepared+involvedw;
   return involvep_ok;
} /* End involvep */
 
 
void uninvolve(key)      /* Maintains backchain order */
/* Input - */
register struct Key *key;   /* Key to uninvolve, not a hook key */
/*
                   The input key must not designate a node prepared as
                   a domain (since we don't check dib->lastinvolved).
 
   Output -
         Involved bits are turned off.
         Key remains prepared but is placed at other end of backchain.
*/
{
   key->type &= ~(involvedr+involvedw);
 
/*
   If key is prepared, then its position on the backchain needs to be
   adjusted. The backchain key order = (Involved,Exit,Other)
*/
   if (key->type & prepared) {
      register NODE *left =    /* The left item for the key */
              (NODE *)key->nontypedata.ik.item.pk.leftchain;
      register NODE *right =   /* The right item for the key */
              (NODE *)key->nontypedata.ik.item.pk.rightchain;
      register NODE *subj =    /* The subject item for the key */
              (NODE *)key->nontypedata.ik.item.pk.subject;
 
      /* Unchain it from where it is */
      right->leftchain = (union Item *)left;
      left->rightchain = (union Item *)right;
 
      /* Chain it into its new place */
      ((NODE *)key)->rightchain = (union Item *)subj;
      ((NODE *)key)->leftchain = (union Item *)subj->leftchain;
      ((NODE *)subj->leftchain)->rightchain = (union Item *)key;
      subj->leftchain = (union Item *)key;
   }
}
 
 
NODE *keytonode(key)
               /* Return the node header for passed key slot */
/* Input */
struct Key *key;            /* Pointer to the slot */
/* Output - Pointer to the node header which contains the slot */
{
/*
  Take difference between key address and beginning of node space
  To get integer number of nodes, we divide by node size.
*/
   register long i = (char *)key - (char *)firstnode;
   i /= sizeof(NODE);
   return firstnode + i;
} /* End keytonode */
