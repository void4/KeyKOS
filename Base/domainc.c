/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "keyh.h"
#include "cpujumph.h"
#include "wsh.h"
#include "prepkeyh.h"
#include "locksh.h"
#include "memomdh.h"
#include "domainh.h"
#include "domamdh.h"
#include "kschedh.h"
#include "timemdh.h"
#include "meterh.h"
#include "memutil.h"

static unsigned long dibcursor = 0;
 
 
/*
  While a domain is prepared, the following statements are true:
  1) Either the caches are zero or slot 1 (the meter) is involved
     and designates a prepared meter node and the caches have been
     deducted from the real counters of each meter of the meter chain.
  5) Slot 15 holds an involved key to a node prepared as a state node
     or a page ...
     the contents of the machine registers, however, are in the
     hardware and/or the dib.  The state page/node is not up to date,
     but if it is a node it holds 16 involved data keys.
*/


unsigned long stealcache(
   struct DIB *dib)
/* Remove cpucache from the dib. Return the value removed. */
{
   unsigned long retval;
   if (dib == cpudibp) { /* it is running */
      uncachecpuallocation();
      set_process_timer(0);
   }
   retval = dib->cpucache;
   dib->cpucache = 0;
   dib->readiness |= ZEROCACHE;
   return retval;
}

static struct DIB *unprn0(     /* Unprepare a node prepasdomain */
   NODE *rn)             /* Node prepared as domain */
/* Returns a pointer to the DIB the domain used. */
{
   struct DIB *dib;
   if (rn->prepcode != prepasdomain)
      crash("DOMAIN005 unprn0 - not prepared as a domain");
 
   dib = rn->pf.dib;
   if (rn->dommeterkey.type & involvedw) {
      retcache(dib);
      uninvolve(&rn->dommeterkey);
   }

/* Copy machine-dependent state, uninvolve memory(s) */
   unpr_dom_md(rn);

   uninvolve(&rn->domprio);             /* Uninvolve priority */
   uninvolve(&rn->domstatekey);         /* Uninvolve state key */
   uninvolve(&rn->domkeyskey);          /* Uninvolve keys key */
/*
  Now set domhookkey
*/
   if (rn->domhookkey.type != pihk) {
      if (dib->readiness & BUSY) rn->domhookkey = dk1;
      else rn->domhookkey = dk0;
   }
/*
  Mark nodes as unprepared
*/
   dib->keysnode->prepcode = unpreparednode;
   rn->prepcode = unpreparednode;
 
   return dib;
} /* End unprn0 */
 
 
static void undo_involve_key(  /* Undoes involving annex key */
   struct Key *key)            /* Pointer to the annex key to uninvolved */
{
   unpreplock_node((NODE *)key->nontypedata.ik.item.pk.subject);
   uninvolve(key);          /* Maintain backchain order */
}


static void undo_involve_dib(
   struct DIB *dib)            /* Pointer to the dib to free */
{
   undo_involve_key(&dib->rootnode->domkeyskey);
   dib->keysnode = NULL;  /* mark it free */
   dib->rootnode = (NODE *)free_dib_head;  /* Free the dib */
   free_dib_head = dib;
}
 
 
int prepdom(          /* Prepare a domain */
   NODE *dr)          /* Root node to be prepared */
              /* Node must be unprepared and preplocked. */
/* cpuactor has actor */
 
/* Output - */
/*   prepdom_prepared  {0}  The key has been prepared */
/*   prepdom_overlap   {1}  The domain overlaps with a
                               preplocked node */
/*   prepdom_wait      {2}  An object must be fetched, actor queued */
/*   prepdom_malformed {3}  The domain is malformed */
{
   NODE *keyn;         /* The keys nodes */
   struct DIB *dib;    /* The domain's dib */

   if (dr->prepcode != unpreparednode)
                  crash("DOMAIN001 prepdom of already prepared node");
/*
   DOMPRIO must be a data key.
   DOMHOOK must be either a HOOK key, or a datakey with the
    11 byte databody equal to 1 or 0.
*/
   if (dr->domprio.type != datakey) 
	return prepdom_malformed;
   if (dr->domhookkey.type != pihk)
      if (dr->domhookkey.type != datakey
          || Memcmp(dr->domhookkey.nontypedata.dk11.databody11,
                    "\0\0\0\0\0\0\0\0\0\0",
                    10)
          /* compare 11'th byte separately, because Memcmp compares
             signed bytes. */
          || dr->domhookkey.nontypedata.dk11.databody11[10] > 1 )
         return prepdom_malformed;

/* Prepare and involve the key to the keys node */

   if (dr->domkeyskey.type != nodekey+prepared &&
           dr->domkeyskey.type != nodekey) 
	return prepdom_malformed;
   switch (involven(&dr->domkeyskey,unpreparednode)) {
    case involven_ioerror:    crash("PREPDOM002 keysnode I/O error");
    case involven_wait:       return prepdom_wait;
    case involven_obsolete:   return prepdom_malformed;
    case involven_preplocked: return prepdom_overlap;
    case involven_ok:         break; /* Key has been involved */
   }
   keyn = (NODE *)dr->domkeyskey.nontypedata.ik.item.pk.subject;

/*
  Acquire a DIB for the domain
*/
   dib = free_dib_head;
   if (dib == NULL) {     /* Steal a dib */
/*
   Since there can only be three domain roots preplocked (in the
   case where the invocation being processed is of a domain key)
   there is bound to be a DIB that can be stolen as long as there
   are more than 3 DIBs.  The loop below terminates ONLY when a
   stealable DIB is located.
*/
      while(1) {
         if (++dibcursor == maxdib) dibcursor = 0;
         dib = &firstdib[dibcursor];
         if (!(dib->readiness & AGEING)) dib->readiness |= AGEING;           
         else if (!preplock_node(dib->rootnode,lockedby_prepdom)) {
            unprn0(dib->rootnode);
            unpreplock_node(dib->rootnode);
      break;
         }
      }
   } /* End steal a dib */
   else free_dib_head = (struct DIB *)dib->rootnode;

/*
  Now link the DIB and the domain components
*/
   dib->keysnode = keyn;
   dib->rootnode = dr;

   {  int rc = prepdom_md(dr, dib);
      switch (rc) {
       case prepdom_prepared: break;
       default:
         undo_involve_dib(dib);
         return rc;
      }
   }

/*
   Finish DIB setup
*/
   dib->readiness = trapcode_nonzero(dib) ? TRAPPED : 0;
   if (dr->domhookkey.type != pihk) {
      dr->domhookkey.type = datakey+involvedr+involvedw;
      if (dr->domhookkey.nontypedata.dk7.databody[6] == 1)
         dib->readiness |= BUSY;
   }
   else {
      dib->readiness |= HOOKED;
      if (dr->domhookkey.databyte == 1)
         dib->readiness |= BUSY;
   }
 
/*
   Compute the midpointer for rn->lastinvolved
*/
   {  register union Item *itm;
      dr->flags &= ~NFREJECT;
      for (itm = dr->rightchain;
           itm != (union Item *)dr;   /* Exit at end of prepared keys */
           itm = itm->item.rightchain) {
         if (itm->key.type != pihk)
      break;                /* Exit at end of hooks */
         /* A hook means there is a stall queue, set flag to show it */
         dr->flags |= NFREJECT;
      }
      dib->lastinvolved = itm->item.leftchain;
   }
/*
   Initialize CACHE and involve the Priority
*/
   dib->cpucache = 0;     /* CPU cache is zero */
   dib->readiness |= ZEROCACHE;
   zap_dib_map(dib);      /* Initialize memory map */
   dr->domprio.type = datakey+involvedr+involvedw;
   dr->prepcode = prepasdomain;
   dr->pf.dib = dib;
   keyn->prepcode = prepasgenkeys;
   keyn->pf.dib = dib;
   unpreplock_node(keyn);
   keyn->flags |= NFDIRTY;
   dr->flags |= NFDIRTY;
   return prepdom_prepared;
}
 
 
void unprdr(dr)             /* Unprepare a domain root node */
/* Input - */
NODE *dr;               /* Pointer to the domain root node */
/*
   Output - None
*/
{
   struct DIB *dib = unprn0(dr);
   dib->keysnode = NULL;  /* mark it free */
   dib->rootnode = (NODE *)free_dib_head;
   free_dib_head = dib;
} /* End unprdr */
