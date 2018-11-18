/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "lli.h"
#include "kktypes.h"
#include "cvt.h"
#include "keyh.h"
#include "wsh.h"
#include "cpujumph.h"
#include "primmeth.h"
#include "prepkeyh.h"
#include "queuesh.h"
#include "dependh.h"
#include "memoryh.h"
#include "memomdh.h"
#include "domamdh.h"
#include "checkh.h"
#include "checmdh.h"
#include "timeh.h"
#include "getih.h"
#include "sparc_check.h"
#include "timemdh.h"
#include "memutil.h"

#define FC(cda) (*(unsigned long long*)((cda)-2) & 0xffffffffffffLL)
 
#define nofmt 0 /* 0 to remove requirement to format before big bang */
 
 
/* The following three fields are defined in SPACEC */
 
extern NODE *freenodes;         /* list of free node frames */
extern CTE  *freepages;         /* list of free page frames */
 

/* The following is defined in timec.c */
extern struct QueueHead bwaitqueue[];

/* The following is defined in sparc_cons.c */
extern struct QueueHead *romconsolequeues;

/* The following is defined in sparc_uart.c */
extern struct QueueHead *lineaqueues;
extern struct QueueHead *linebqueues;
 
 
/* The following field is defined in KSCHEDC */
 
extern struct DIB idledib;
 
 
char cdaofprimemeter[6] = {0,0,primemetercda>>24 & 0xff,
                                 primemetercda>>16 & 0xff,
                                 primemetercda>>8 & 0xff,
                                 primemetercda & 0xff};
static long numberofnodes;
 
char memobject;
unsigned char ckseapformat;
uint64 cvaddr;
NODE *chbgnode;
long entrylocator;
unsigned short entryhash;

static int tzcda(short * p){return (*p | *(int*)(p+1));}

uint64
__lshrdi3(uint64 a, uint32 n) {
  while (n--) a >>= 1;
  return a;
}
 
static void clearslots(void)
/*
  CLEARSLOTS
 
         This routine visits every slot of every node that has
         a prepared key in it.  The CHECKMARK field of the key
         is cleared.
 
         The companion routine CHECKSLOTS crashes if it finds the
         CHECKMARK field is zero.
 
         This mechanism is used to insure that all keys on chains
         are prepared and that all prepared keys are on chains at
         least once and at most once.
*/
{
   NODE *np;             /* The current node */
   struct Key *key;      /* The current key */
 
   for (np = firstnode; np < anodeend; np++) {
/* 9 demerits for having been caught by sampler! */
      for (key = &np->keys[0]; key <= &np->keys[15]; key++) {
         if (key->type & prepared)
                    key->checkmark = 0;
      }
   }
}
 
 
static void checkslots(void)
/*
  CHECKSLOTS - See CLEARSLOTS above.
*/
{
   NODE *np;             /* The current node */
   struct Key *key;      /* The current key */
 
   for (np = firstnode; np < anodeend; np++) {
      if (tzcda((short*)&np->cda))
         /* Not a free node */
         for (key = &np->keys[0]; key <= &np->keys[15]; key++) {
            if ( (key->type & prepared) &&
                       !key->checkmark)
               crash("CHECK001 checkslots - key not marked");
         }
   }
}
 
static void checkdesignee(   /* Checks a backchain */
   union Item *root,      /* The left pointer of the backchain */
   void (*proc)(               /* Procedure to further check each key */
       struct Key *key))
/*
  CHECKDESIGNEE
 
  This routine considers the backchain rooted at slot *root.
  It marks each key on the chain.
*/
{
   union Item *key, *ri;
 
/*
      Loop through all keys on the backchain of this node
*/
   for (ri = root; ; ri = key) {
      long nodeindex;
      NODE *kn;             /* The node that the key is in */
 
      key =          /* The item left of ri */
            ri->item.leftchain;
 
/*
  Get Left pointer, see if right pointer of Left key equals key.
*/
      if (ri != key->item.rightchain)
         crash("CHECK002 right of left of item NE item");
 
/*
  if Left pointer equals root we are at the end of the chain
*/
      if (key == root)
   break;
 
/*
   See if the Left Chain pointer (key) is indeed to a key in a node
   frame by computing the node frame based on the address.
*/
      nodeindex = ((char *)key-(char *)firstnode)/sizeof(NODE);
      if (nodeindex > numberofnodes || nodeindex<0)
         crash("CHECK003 Key not in a node");
 
/*
  key should point to one of the slots in the node frame between
  slot 0 and slot 15  */
      kn = firstnode + nodeindex;
      if (   (struct Key *)key < kn->keys
          || (struct Key *)key > kn->keys+15 )
         crash("CHECK004 Key not within slots of node");
 
      if (((char *)key-(char *)kn->keys) % sizeof (struct Key))
         crash("CHECK005 Key doesn't start at slot boundary");
 
/*
  This next operation marks the key as on some chain
  and tests that it is not on more than 1 chain.  At the
  the end of CHECK there is a test that all prepared keys
  must be on some chain.  All keys on the chain must
  have root as the Subject.
*/
      if (key->key.checkmark)
         crash("CHECK006 Key on two back chains");
 
      key->key.checkmark = 0xff;
 
      if (key->key.nontypedata.ik.item.pk.subject != root)
         crash("CHECK007 Key on back chain doesn't designate subject");
 
      if ((key->key.type & prepared) == 0)
         crash("CHECK008 Key on back chain not marked prepared");
 
      (*proc)((struct Key *)key);
          /* Perform further key checks (depends on caller) */
 
   } /* End for all on back chain */
}
 
static void (*next_node_backchain)(  /* Procedure variable used below */
#ifdef prototypes
   struct Key *key
#endif
   );
 
/* The following are assigned to next_node_backchain */
 
static void node_involved(
   struct Key *key)
{  if (key->type&(involvedw|involvedr)) {
      if (key->type&involvedr && !(key->type&involvedw) )
         crash("CHECK009 Involvedr key which is not involvedw");
      return;
   }
   crash("CHECK010 node backchain out of order");
}
static void node_resumes(
   struct Key *key)
{  if (key->type&(involvedw|involvedr)) {
      next_node_backchain = node_involved;
      node_involved(key);
   }
}
static void node_others(
   struct Key *key)
{  if (key->type&(involvedw|involvedr) || key->type==resumekey+prepared) {
      next_node_backchain = node_resumes;
      node_resumes(key);
   }
}
 
 
static void ckdesignee_node(   /* Check keys on a nodes backchain */
   struct Key *key)
/*
  This routine checks the type of keys on a node's backchain. It is
  passed to checkdesignee as a procedure parameter. N.B. THE CALLER
  OF CHECKDESIGNEE MUST SET THE PROCEDURE VARIABLE next_node_backchain
  to node_others before calling checkdesignee.
 
  The backchain is ordered from the Left pointer with non-exit,
  non-involved keys first, then exit keys and then involved keys.
 
  next_node_backchain is a procedure variable that is set to
  first node_others, then node_exits, and then node_involved.
  it is used to check the backchain order.
*/
{
   switch (key->type & keytypemask) {
    case datakey:
    case nrangekey:
    case prangekey:
    case chargesetkey:
    case devicekey:
    case pagekey:
    case misckey:
    case copykey:
      crash("CHECK011 Bad key type on node backchain");
    case startkey:
    case resumekey:
    case segmentkey:
    case nodekey:
    case meterkey:
    case fetchkey:
    case domainkey:
    case hookkey:
    case sensekey:
    case frontendkey:
      break;
    default: crash("CHECK012 Key->type out of range");
   }
/*
  Key type is valid for a key on a node's backchain. Now call
  the correct routine based on next_node_backchain.
  It changes to the next procedure only when a key fails to pass
  the curret procedure's test.  In this way, for a well-formed chain,
  each test should fail (and fall through) only once.
*/
   (*next_node_backchain)(key);
}
 
 
static void ckdesignee_hook( /* Check keys on a Queue's backchain */
   struct Key *key)
/*
  This routine checks the type of keys on a queue's backchain. It is
  passed to checkdesignee as a procedure parameter
*/
{
   if (key->type != pihk) crash("CHECK013 Non-hook on queue backchain");
}
 
static void checkqueuehead(   /* Checks a queue head */
   struct QueueHead *qh)
{
   checkdesignee((union Item *)qh, ckdesignee_hook);
}
 
 
static void ckdesignee_page(   /* Check keys on a CTE's backchain */
   struct Key *key)
/*
  This routine checks the type of keys on a CTE's backchain. It is
  passed to checkdesignee as a procedure parameter
*/
{
   if ((key->type & (keytypemask|prepared)) != pagekey+prepared)
      crash("CHECK116 Non-prepared page key on CTE backchain");
}
 
 
Producer *check_memory_key(    /* Check a memory key */
      struct Key *key)                 /* The key to check */
/*
   Additional Input -
      ckseapformat has read-only and no-call bits set
 
   Output -
      memobject is 0 if key designates a node, else 1 for cte
      ckseapformat is updated for this key (r/o, n/c + lss)
      returned value is the object that the key designates.
 
   Each memory key consulted in memory tree must pass this test!
*/
{
   if (key->type == pagekey+prepared+involvedw) {
      memobject = 1;
   }
   else {
      if     (key->type != nodekey+prepared+involvedw
           && key->type != fetchkey+prepared+involvedw
           && key->type != sensekey+prepared+involvedw
           && key->type != segmentkey+prepared+involvedw)
         crash("CHECK014 Wrong key type in memory tree");
      memobject = 0;
   }
/*
   Accumulate access restrictions with new lss as we descend the tree.
*/
   ckseapformat &= 0xc0;        /* Keep read-only and no-call bits */
   ckseapformat |= key->databyte;
   return (Producer *)key->nontypedata.ik.item.pk.subject;
}
 
 
Producer *check_memory_tree(     /* Check memory tree */
      Producer *item,              /* Node or page to check */
      int limit,                   /* Extent limit to check */
   unsigned int slot_origin)     /* slot/2 to start with */
/*
   Additional Input -
      ckseapformat has read-only and no-call bits and lss from key
      memobject is 0 if key designates a node, else 1 for page
      chbgnode is pointer to node with background key or NULL
      entrylocator and entryhash describe the entry.
      cvaddr has the effective address
 
   Output -
      cvaddr is updated
      ckseapformat is updated for this key (r/o, n/c + lss)
      returned value is new memory key or cte at leaf
 
   This routine executes the memory tree algorithm rather directly
   for some portion of a memory tree. Like routine SEAP in memory
   it scans down a memory access path thru nodes to determine
   if one of several types of fields has the right value.
 
   In each case the scan is terminated when the "extent limit"
   is reached. This is an SSC threshold that depends on the field type.
 
   CHECKDEP is called for each consulted slot to ensure that DEPEND
   holds all entries required.
*/
{
   int exitlimit = limit;
   unsigned int origin = slot_origin;

/*
   BEGIN EXAMINE_EACH_MEMORY_KEY_IN_PATH_PORTION
 
   This body of code steps thru a memory key to the next lower
   key in the memory tree. It exits when the SSC of that
   key does not exceed EXLIM.
 
   This code corresponds to SEAPLOOP in MEMORY. It is simpler because
   it need not lock nodes nor summon absent nodes or pages.
*/
   for (;;) {
      struct Key *lastinitial;     /* Last initial slot */
      NODE *n;
      int ssc;
      struct Key *nextkey;         /* The next key down the path */
 
      if (memobject == 1) {
         return item;
      } /* End item is a core table entry */
 
      n = &item->node;
/*
      IT IS A NODE, Best be prepared as a segment
*/
      if (n->prepcode != prepassegment)
         crash("CHECK015 Memory node not prepared as segment");
 
      if (!(ckseapformat & 15)) {        /* Node is red */
         if (n->keys[15].type != datakey+involvedw)
            crash("CHECK016 format key not involvedw datakey");
 
         if (depend_check_entries(n->keys+15, entrylocator, entryhash))
            crash("CHECK018 No depend entry for format key");
  
         ssc = (n->keys[15].nontypedata.dk6.databody6[5] & 15);
 
         lastinitial = n->keys - 1 +
               (n->keys[15].nontypedata.dk6.databody6[5] >> 4);
      } /* End segment is red */
 
      else {
         lastinitial = n->keys+15;
         ssc = ckseapformat & 15;
      }
      if (ssc < 3 || ssc > 12)
         crash("CHECK019 out of range lss/ssc found");
 
      if (ssc <= exitlimit) {
         return item;
      } /* End exit because of extent limit */
/*
      We have ascertained that this node's SSC (in ssc)
      is valid and that we must proceed further down the memory tree.
*/
      {
         uint64 saddr = cvaddr;
 
/*
         Isolate nibble of address that indexes into this segment node.
*/
         saddr >>= ssc*4; /*llilsr(&saddr, ssc*4);*/
         if (saddr > 15)
            crash("CHECK020 Memory tree slot index > 15");
 
         nextkey = n->keys + origin*2 + saddr;
         if ((nextkey->type & involvedw) == 0) {
            crash("CHECK097 Memory key not involved");
            if(0){if ((nextkey->type != pagekey))
                  crash(" (not a page key)");
               if (depend_check_entries(nextkey, entrylocator, entryhash))
                  crash(" w/ depend error");
               else crash(", depend ok");}
         }
         if (nextkey > lastinitial)
            crash("CHECK021 used slot beyond initial slots");
 
         saddr <<= ssc*4; /* Remove index just */
         cvaddr -= saddr; /* used from address */
      }
 
      if (depend_check_entries(nextkey, entrylocator, entryhash))
         crash("CHECK023 No depend entry for key");
 
      if (n->prepcode != prepassegment)
         crash("CHECK024 segment node not prepared as segment");
 
      if (!(ckseapformat & 15) &&                  /* Node is red */
            (n->keys[15].nontypedata.dk7.databody[4]&15) != 15 ) {
/*
         Note background key here.
*/
         chbgnode = n;
      } /* End node is red (part 2) */
 
      if (nextkey->type == datakey+involvedw) {
/*
         KEY is some kind of window key:
*/
         if ((nextkey->nontypedata.dk7.databody[6] & 2) == 0)
            crash("CHECK026 window key id bit is zero");
 
/*
        Accumulate access restrictions as we descend the tree.
*/
        ckseapformat |= (nextkey->nontypedata.dk7.databody[6]
                       << 4) & 0xc0;
 
/*
        Apply offset from window key to address.
*/
//Panic(8);
        {
	/*	uint64 msk = ~0<<ssc*4, ofst = *(uint64*) HACK: */
	uint64 msk = ~0LL<<ssc*4, ofst =
	
	// *(uint64*)(nextkey->nontypedata.dk11.databody11 + 3)&~0xFFF;
// The line aove was replaced by the two lines below for a machine that could
// not do an unaligned 64 bit load. The code below may be more efficient even
// when the hardware can cope.
	  ((uint64)*(uint32*)(nextkey->nontypedata.dk11.databody11 + 3))<<32
	| ((uint64)*(uint32*)(nextkey->nontypedata.dk11.databody11 + 7)&~0xfff);
	
           if(cvaddr & msk) crash("can't happen");
           if(ofst & ~msk) crash("Window alignment violation");
           cvaddr |= ofst;}
          
        if (nextkey->nontypedata.dk11.databody11[10] & 1) {
           /* A background window key */
           if (nextkey->nontypedata.dk11.databody11[10] & 0xf0)
              crash("CHECK027 background window key w/slot select");
           nextkey = chbgnode->keys +
              (chbgnode->keys[15].nontypedata.dk7.databody[4] & 15);
         } /* End background window key */
         else {          /* Local window key */
            nextkey = n->keys + (nextkey->nontypedata.dk7.databody[6]>>4);
         } /* End local window key */
 
/*
         nextkey -> key appointed by window key.
*/
         if ((nextkey->type&(involvedw|prepared))!=involvedw+prepared)
            crash("CHECK028 Window key's key not prepared+involved");
 
         if (depend_check_entries(nextkey, entrylocator, entryhash))
            crash("CHECK030 No depend entry for window key");
 
      }  /* End KEY is some kind of window key */
 
      item = check_memory_key(nextkey);
      origin = 0;
 
   } /* End for walk the tree */
 
} /* End check_memory_tree */
 
 
static void check_nonfree_node(
   register NODE *np)
{
   struct Key *key;
 
/*
   The first test of a node is that it should not be locked
   In an MP, CHECK will have to get the other CPUs to gather
   around and divide up the tasks of CHECK
*/
   if (np->preplock & 0x80) crash("CHECK032 Preplocked node");
   if (np->corelock) {
      if (Memcmp(np->cda, cdaofprimemeter, sizeof np->cda))
          crash("CHECK033 Corelocked node");
   }
   if (np->allocationid == 0
       && !(np->flags & NFALLOCATIONIDUSED))
      crash("CHECK386 node allocationid zero, not used");
 
/*
   See if this CDA is in its hash chain more than once
   The Node HASH chains terminate with a pointer to NULL.
*/
 
   {  register NODE *n, *fn = NULL;
 
      for (n = anodechainheads[cdahash(np->cda)
                               & nodechainhashmask ];
           n;
           n = n->hashnext) {
 
         if (n<firstnode || n >= anodeend)
            crash("CHECK035 Node hash chain outside node space");
 
         if (Memcmp(np->cda, n->cda, 6) == 0) { /* cda match */
            if (fn) crash("CHECK036 Two nodes with same CDA");
            fn = n;
         } /* End cda match */
 
/*
         fn has the address of the node frame found on the hash
         chain with the CDA in question.  There was at most 1.
         fn may be zero if the node frame was not found.  If fn
         is not the same as np then some other node frame with the
         same CDA was found.
*/
      } /* End check node hash chain */
 
      if (np != fn) crash("CHECK037 Our node not in hash chain");
   }
 
/*
   CHECK BACKCHAIN
 
   CHECKDESIGNEE validates all the keys on backchain rooted
   in the Left pointer of the Node.  They should all be
   in nodes and have a Subject of this node.
 
   CHECKDESIGNEE_NODE checks their key types and the backchain order.
*/
   next_node_backchain = node_others;
   checkdesignee((union Item *)np,ckdesignee_node);
 
 
/*
   BEGIN CHECK_ALL_KEYS_IN_THE_NODE
 
   Now each key in the node is examined and tested for validity.
*/
   for (key = np->keys; key<=np->keys+15; key++) {
      switch (key->type & keytypemask) {
 
 
       case chargesetkey:
/*
         Charge Set keys seem to require that the first byte is zero
*/
         if (key->nontypedata.dk7.databody[0] != 0)
            crash("CHECK038 Chargeset key databody[0] != 0");
 
         /* Fall thru for unprepared check */
 
 
       case datakey:
       case nrangekey:
       case prangekey:
       case devicekey:
       case copykey:
/*
         These keys may not be prepared
*/
         if (key->type & prepared)
            crash("CHECK039 Key type should not be prepared");
 
         break;
 
 
       case misckey:
/*
         The first byte of the databody is the TYPE and must be even
*/
         if (key->nontypedata.dk11.databody11[0] & 1)
            crash("CHECK040 MISC key with odd subtype");
 
         if (key->type & prepared)
            crash("CHECK041 Key type should not be prepared");
 
         break;
 
 
       case pagekey:
/*
         If a page key is NOT prepared, the page may not be in
         core.  SRCHPAGE is called to see.  If it is in core,
         then the Allocation ID is checked in the Core table entry.
         The key may have a lower Allocation ID else it must be equal
         and the Allocation Used must be 1.
 
         If the key is prepared the subject pointer should be a
         core table entry.  The subject pointer is checked against
         FIRSTCTE and APAGEEND.
*/
         if (key->databyte & ~readonly)
            crash("CHECK042 page key databyte & ~readonly != 0");
 
         if (key->type & prepared) { /* Page key prepared */
            CTE *sub = (CTE *)key->nontypedata.ik.item.pk.subject;
 
            if (sub < firstcte || sub >= lastcte)
               crash("CHECK043 Prep page subject outside coretable");
         }
 
         else {            /* Page key not prepared */
#if nofmt /* This check is temporarily disabled because
      if you do a big bang without formatting the disk(s),
      old nodes can exist with old keys with old allocationid's. */
            CTE *cte = srchpage(key->nontypedata.ik.item.upk.cda);
 
            if (cte) {
               if (cte->use.page.allocationid <
                        key->nontypedata.ik.item.upk.allocationid)
                  crash("CHECK044 cte->allo_id > key->allo_id");
               if (cte->use.page.allocationid ==
                        key->nontypedata.ik.item.upk.allocationid
                     && !(cte->flags & ctallocationidused))
                  crash("CHECK045 CTE ctallocationidused != 1");
            }
#endif
         }
 
         break;
 
 
       case domainkey:
/*
         The databyte should be zero
*/
         if (key->databyte)
            crash("CHECK046 Domain key databyte not zero");
         goto nodetypekey;
 
 
       case meterkey:
/*
         Meter keys must not be InvolvedR and if they are InvolvedW
         they must be prepared unless they are to Super Meter which
         is the superior meter to the Prime Meter.  If the meter key
         is prepared, then the subject (node) must be prepared as a
         meter.
*/
         if (key->databyte)
            crash("CHECK047 Meter key databyte not zero");
         if (key->type & involvedr)
            crash("CHECK048 Meter key found involvedr");
         if (!(key->type & prepared) /* if not prepared */
             && !tzcda((short*)&key->nontypedata.ik.item.upk.cda) ) /* and cda == 0 */
            /* the super meter key */
            break;
         if (key->type & involvedw) {
            if (key->type & prepared) {
               NODE *s = (NODE*)key->nontypedata.ik.item.pk.subject;
 
               if (s->prepcode != prepasmeter)
                  crash("CHECK049 Involved meter key node not prep");
            }
            else {
               crash("CHECK050 involved meter key not prepared");
            }
         }
         goto nodetypekey;
 
 
       case hookkey:
/*
         Hook keys must be Prepared and Involved and must be in
         slot 13.  If the Subject is the worry queue the databyte
         must be worry_hook else it must be process_hook. If the
         subject is a node then the hook must be a STALLHOOK and the
         subject must be a node frame. This is checked with KEYTONODE
*/
         if (key->type != pihk)
            crash("CHECK051 Hook key not prepered & involved");
 
         if (key != &np->domhookkey )
            crash("CHECK052 Hook key not in domhookkey slot");
 
         if (key->nontypedata.ik.item.pk.subject
                  == (union Item *)&worryqueue) {
            if (key->databyte != worry_hook)
               crash("CHECK053 Worry hook databyte wrong");
         }
         else {
            if (key->databyte != process_hook)
               crash("CHECK054 Process hook databyte wrong");
         }
         {  NODE *sub = (NODE *)key->nontypedata.ik.item.pk.subject;
 
            if (sub >= firstnode && sub < anodeend) {  /* Stall hook */
/*
               Call KEYTONOD to see if subject designates a node
*/
               if (keytonode(sub->keys) != sub)
                  crash("CHECK056 Stallhook subject != node");
            }
         }
         break;
 
 
       case resumekey:
/*
         The Databyte of an Resume key must be 0, 2 or 4.  If the key
         is prepared it must designate a node frame.
         Else SRCHNODE is called to see if the subject is in core
         anyway.  If so then the call ID is checked.
         The key may have a lower Call ID else it must be equal
         and the NFCALLIDUSED must be 1.
*/
         switch (key->databyte) {
          default: crash("CHECK057 Bad resume key databyte");
          case restartresume:
          case returnresume:
          case faultresume:
          case reskck2: case reskck3:
            break;
         }
         if (key->type & prepared) {
            NODE *sub = (NODE *)key->nontypedata.ik.item.pk.subject;
/*
            Call KEYTONOD to see if subject designates a node
*/
            if (keytonode(sub->keys) != sub)
               crash("CHECK058 Stallhook subject != node");
         }
         else {
#if nofmt /* This check is temporarily disabled because
      if you do a big bang without formatting the disk(s),
      old nodes can exist with old keys with old allocationid's. */
            NODE *n = srchnode(key->nontypedata.ik.item.upk.cda);
 
            if (n) {
               if (n->callid <
                        key->nontypedata.ik.item.upk.allocationid)
                  crash("CHECK059 n->call_id > key->call_id");
               if (n->callid ==
                        key->nontypedata.ik.item.upk.allocationid
                     && !(n->flags & NFCALLIDUSED))
                  crash("CHECK060 NODE callidused != 1");
            }
#endif
         }
         break;
 
 
       case startkey:
       case segmentkey:
       case nodekey:
       case fetchkey:
       case sensekey:
       case frontendkey:
/*
  KEY DESIGNATES A NODE.
 
         If the key is prepared then the subject must be a node.
         If it is not then the subject is checked as in unprepared
         exits and pages.
*/
nodetypekey:
         if (key->type & prepared) {
            NODE *sub = (NODE *)key->nontypedata.ik.item.pk.subject;
/*
            Call KEYTONOD to see if subject designates a node
*/
            if (keytonode(sub->keys) != sub)
               crash("CHECK061 Node type key subject != node");
         }
         else {
#if nofmt /* This check is temporarily disabled because
      if you do a big bang without formatting the disk(s),
      old nodes can exist with old keys with old allocationid's. */
            NODE *n = srchnode(key->nontypedata.ik.item.upk.cda);
 
            if (n) {
               if (n->allocationid <
                        key->nontypedata.ik.item.upk.allocationid)
                  crash("CHECK062 n->alloc_id < key->alloc_id");
 
               if (n->allocationid ==
                        key->nontypedata.ik.item.upk.allocationid
                     && !(n->flags & NFALLOCATIONIDUSED))
                  crash("CHECK063 NODE allocationidused != 1");
            } 
            /* Else a key to a node that happens not to be
               in memory. */
#endif
         }
         break;
 
 
       default: crash("CHECK064 Key->type out of range");
      } /* End switch on key->type */
 
   } /* End check_a_key */
   /* End check_all_keys_in_the_node */
 
 
 
/*
   Now look at the node itself.  There are 6 valid prepared states
*/
   switch (np->prepcode) {
    case unpreparednode:
/*
      UNPREPARED NODE may have no involved keys except a hook key
*/
      for (key = np->keys; key<=np->keys+15; key++) {
         if (key->type & (involvedr|involvedw)
             && key->type != pihk)
            crash("CHECK065 Involved non-hook in unprepared node");
      }
      break;
 
 
    case prepasdomain:        /* Domain root */
      {  struct DIB *dib = np->pf.dib;
 
/*
         First see if the DIB points to the Node
*/
         if (dib->rootnode != np)
            crash("CHECK066 Dib->rootnode != node");
 
         if (dib->cpucache){
            if(np->dommeterkey.type != (meterkey | prepared | involvedw))
               crash("dom with cache lacks meter.");
            if(((NODE*)(np->dommeterkey.nontypedata.ik.item.pk.subject))
                 -> pf.drys & 0x80) crash("wet domain under dry meter.");}
         else if (!(dib->readiness & ZEROCACHE)) crash("ZEROCACHEo");
 
/* Check machine-dependent stuff */
         check_prepasdomain_md(dib);
/*
         Validate the ordering of the back chain.  All the Involved
         keys are between RIGHTCHAIN and LASTINVOLVED.
*/
         {
            union Item *lastinv = (union Item *)np;
 
            for (;;) {
               union Item *key2 = lastinv->item.rightchain;
               if (key2 == (union Item *)np)
            break;
               if (!(key2->key.type & (involvedr|involvedw)))
            break;
               if (key2->key.type != pihk)
                  crash("CHECK085 Involved !hook on dom backchain");
 
               lastinv = key2;
            } /* End for all involved on back chain */
            if (lastinv != dib->lastinvolved)
               crash("CHECK086 dib->lastinvolved error");
            if (lastinv == (union Item *)np) { /* No stallees */
               if (np->flags & NFREJECT)
                  crash("CHECK087 node->flags reject in error");
            }
            else {
               if (!(np->flags & NFREJECT))
                  crash("CHECK088 node->flags reject in error");
            }
         }
/*
         The next two checks are to make sure that READINESS and
         traps and hooks agree.
*/
         /* CHECK "TRAPPED" */
         if (!!(dib->readiness & TRAPPED) != !!trapcode_nonzero(dib))
            crash("CHECK089 TRAPPED in readiness wrong");
 
         /* CHECK "HOOKED" */
         if ((np->domhookkey.type == pihk) != !!(dib->readiness & HOOKED)) 
            crash("CHECK089 HOOKED in readiness wrong");        
        
      } /* End prepared as a domain */
      break;
 
 
    case prepassegment:
      check_prepassegment_md(np);  /* machine-dependent check */
      break;
 
 
    case prepasstate:
      check_prepasstate_md(np);
      break;
 
 
    case prepasgenkeys:
      {  struct Key *k;     /* All keys must be uninvolved */
         for (k = np->keys; k <= &np->keys[15]; k++) {
            if (k->type & (involvedr|involvedw) &&
                    k->type != pihk)
               crash("CHECK096 Involved key in keys node");
         }
      }
      break;
 
 
    case prepasmeter:
/*
      Check validity of the meter
*/
      if (np->dommeterkey.type == meterkey + involvedw) {
         if (tzcda((short*)&np->dommeterkey.nontypedata.ik.item.upk.cda))
             crash("CHECK097 Nonpreped superior not supermeterkey");
      }
      else {
         NODE* unp = (NODE*)np->dommeterkey.nontypedata.ik.item.pk.subject;
         if (np->dommeterkey.type != meterkey+prepared+involvedw)
            crash("CHECK098 Superior not preped involved meter key");
         if (unp->prepcode != prepasmeter)
                 crash("meter don't point to meter!");
         if (!np -> pf.drys && unp-> pf.drys) crash("Dry on top");
         if (np ->meterlevel != unp->meterlevel + 1) Panic();
         if (np->keys[3].type != datakey+involvedr+involvedw) Panic();
      }
      break;
 
 
    default: crash("CHECK099 Invalid node preperation code found");
   } /* End switch on node preparation code */
}

void check(void)
/*
  CHECK - Do consistency checking
 
          CHECK is called before every checkpoint and insures
          that the kernel data structures describe a correct
          system state.  Any inconsistency will cause a crash
 
          The major sections of CHECK are:
              CHECK_ALL_NODES
              CHECK_ALL_QUEUE_HEADS
              CHECK_ALL_DEVICEBLOCKS
              CHECK_ALL_PAGES
              REVIEW_ALL_MAP_ENTRIES
              REVIEW_ALL_ASBS
 
*/
{
   NODE *np;             /* The current node */
   static int times_checked = 0;
   ++times_checked;

   check_caches();

/*
 ======================================================================
 
   Set up to scan all Node Frames.  Nodes are checked relative
   to Hash Chains and backchains.  All the keys in nodes are
   also checked.
 
   First check that Node Space is an integral number of
   frames in size.  Calculate the number of node frames in node space
*/
   numberofnodes = ((char*)anodeend-(char*)firstnode) / sizeof(NODE);
   if ( ((char*)anodeend-(char*)firstnode) % sizeof(NODE) )
      crash("CHECK031 Node space not multiple of node size");
 
/*
 ======================================================================
 
   CLEARSLOTS is called to clear the CHECKMARK flag in all prepared
   keys in nodes.
 
   Later in CHECK CHECKSLOTS will be called to see that all prepared
   keys have been visited.
*/
   clearslots();
 
/*
 ======================================================================
 
  BEGIN CHECK_ALL_NODES
*/
/*
   BEGIN MARK_ALL_NODES_IN_THE_FREELIST
*/
   for (np = freenodes; np; np = np->hashnext) {
      if(np->corelock) crash("Free node locked");
      np->corelock = 0x80;
      if(*(int *)(np->cda+2)) crash("Free node with cda>0");
   }
/* END MARK_ALL_NODES_IN_THE_FREELIST */
 
   for (np = firstnode; np < anodeend; np++) {
 
/*
      BEGIN CHECK_A_NODE
      np -> NODE
*/
/*
      Any node frame with a zero CDA is free and should be on the free
      list.  If the CDA is not zero the hash chain is searched for
      duplicate entries.
*/
      if (!tzcda((short*)&np->cda)) {
/*
         Check that it is on the free list
*/
         if (np->corelock != 0x80)
            crash("CHECK034 free node not on the free list");
         np->corelock = 0;  /* reset the "on freelist" flag */
/*
         Check that its backchain is empty.
*/
         if (np->leftchain != (union Item *)np ||
             np->rightchain != (union Item *)np)
            crash("CHECK117 free node has bad backchain");
      }
      else     /* node does not have CDA zero */
         check_nonfree_node(np); /* this is a subroutine in order
            to gain some indenting space */
        /* End node does not have CDA zero */
   } /* END CHECK_ALL_NODES */
 
/*
 ======================================================================
 
  BEGIN CHECK_ALL_QUEUE_HEADS
 
  Now look at all nodes on the kernel queues.  Verify the
  backchains the same way as when we found a queue through a node.
  We are visiting more prepared keys.
*/
   {
      struct QueueHead *qh;
      int i;

      checkqueuehead(&cpuqueue);
      checkqueuehead(&frozencpuqueue);
      checkqueuehead(&migratewaitqueue);
      checkqueuehead(&migratetransitcountzeroqueue);
      checkqueuehead(&junkqueue);
      checkqueuehead(&worryqueue);
      checkqueuehead(&rangeunavailablequeue);
      checkqueuehead(&kernelreadonlyqueue);
      checkqueuehead(&noiorequestblocksqueue);
      checkqueuehead(&nonodesqueue);
      checkqueuehead(&nopagesqueue);
      checkqueuehead(&resyncqueue);

      qh = romconsolequeues;
      for (i=0; i<2; i++) {
         checkqueuehead(qh);
         qh++;
      }
#ifdef LINEA
      qh = lineaqueues;
      for (i=0; i<2; i++) {
         checkqueuehead(qh);
         qh++;
      }
#endif

      qh = linebqueues;
      for (i=0; i<2; i++) {
         checkqueuehead(qh);
         qh++;
      }
 
      qh = ioqueues;
      for (i=0; i<NUMBERIOQUEUES; i++) {
         checkqueuehead(qh);
         qh++;
      }
 
      qh = bwaitqueue;
      for (i=0; i<nbwaitkeys; i++) {
         checkqueuehead(qh);
         qh++;
      }
   } /* END CHECK_ALL_QUEUE_HEADS */
/*
*======================================================================
*
* BEGIN CHECK_ALL_DEVICEBLOCKS
*
* There can be queues on devices waiting for I/O to begin
* so we check these to visit the last of the prepared keys
* and validate the more of the chains.
*/ /*
         L R11,=V(#DEVICES)
         L R11,0(R11)        NUMBER OF DEVICE BLOCKS
         SLR R10,R10         DEVICE INDEX
         DO COUNT,(R11)
           LR R1,R10
           SLL R1,2          *4
           L R2,=V(DEVPTRS)
           L R1,0(R1,R2)     GET DEVICE BLOCK
           USING DEVICE,R1
           LA R1,DEVWAITQUEUE
           DROP R1
           BAL R14,CHECKDESIGNEE
           LA R10,1(,R10)
         ENDDO ,
* END CHECK_ALL_DEVICEBLOCKS
*/
 
 
/*
 ======================================================================
 
   BEGIN CHECK_ALL_PAGES
 
   Pages on the free list are chained through hashnext of the
   CTE.  Visit all the CTEs for these pages and make sure that
   no page is on the list twice nor any indication in the CTE
   that the page is not free.
*/
   {
      CTE *cte;
/*
      BEGIN MARK_ALL_PAGES_IN_THE_FREELIST
*/
      for (cte = freepages; cte; cte = cte->hashnext) {
         if(!(firstcte <= cte && cte < lastcte)
             || ((u_int)cte-(u_int)firstcte)%sizeof(CTE))
                 crash("Wild freepage list");
         if(cte->ctefmt != FreeFrame) crash("Bad code in Free page");
         if(cte->corelock && FC(cte->use.page.cda) != 1)
              crash("Locked free page");
         cte->corelock = 0x80;
      } /* End MARK_ALL_PAGES_IN_THE_FREELIST */
 
/*
      Now check each Core Table Entry
*/
      for (cte = firstcte; cte < lastcte; cte++) {
/*
         BEGIN CHECK_A_PAGE
 
         For each Core Table Entry determine its type.
         If a Page, then see if the CDA is zero or not
            If CDA=0, see if on the free list
            If CDA not = 0
               check hash chains, and all asbs produced by this page
         If a Pot, we don't do any special checking.
*/
         switch (cte->ctefmt) {
          case NodePotFrame: break;
          case AlocPotFrame: break;
            /* We no longer check pots */
            break;  /* end of PotFrame type */
          case PageFrame:
            if (!tzcda((short*)&cte->use.page.cda))
               crash("check987 PageFrame with CDA == zero");
/*
            Page is either Ordinary or a backup version of the page
*/
            if ((cte->flags & ctbackupversion) == 0) { /* not bu */
               checkdesignee((union Item *)cte, ckdesignee_page);
               if(cte->use.page.maps) check_page_maps(cte);
            }
/*
            SEE IF THIS CDA IS IN ITS HASH CHAIN MORE THAN ONCE
            The CDA may be in the hash chains more than once as there
            are two versions of the page (ordinary and backup)
            Only if the CDA is in the chains twice as the same kind
            of page is it an error
*/
            {
               CTE *c, *found = NULL;

               for (c = apagechainheads[cdahash(cte->use.page.cda)
                                        & pagechainhashmask ];  // Snail alert!!
                    c;
                    c = c->hashnext) {
                  if (c < firstcte || c >= lastcte)
                     crash("CHECK101 hash chain outside coretable");
                  if (Memcmp(c->use.page.cda,
                                 cte->use.page.cda, 6) == 0) {
/*
                     Entry has same cda as our page does
*/
                     if (PageFrame == c->ctefmt) {
                        if (c->flags & ctbackupversion) {
                           /* Page is a backup version */
                           if (cte->flags & ctbackupversion
                               && !((cte->extensionflags
                                     ^ c->extensionflags)
                                    & ctwhichbackup)) {
                              /* Page is same backup version */
                              if (found)
                                 crash("CHECK102 page hashed twice");
                              found = c;
                           } /* End page is same backup version */
                        }
                        else {      /* Page is not backup version */
                           if ((cte->flags & ctbackupversion) == 0) {
                              /* Both pages current version */
                              if (found)
                                 crash("CHECK103 page hashed twice");
                              found = c;
                           } /* End both pages current version */
                        } /* End page is not backup version */
                     } /* End frame is a PageFrame */
                  } /* End page with same CDA */
               } /* End for all pages on hash chain */
               if (!found) crash("CHECK104 Page not on hash chain");
            } /* End check page hash chain */
 
            scan_mapping_tables_for_page(cte);
            break;
          case FreeFrame:
            if (cte->iocount != 0)
               crash("check574 Free page with iocount !=0");
            if (cte->extensionflags & ctkernellock)
               crash("check575 Free page with ctkernellock");
            if (cte->corelock != 0x80)
               crash("CHECK105 Free page not on free list");
            cte->corelock = 0;
            break;
          case InTransitFrame:
            if (!(cte->extensionflags & ctkernellock))
               crash("check577 InTransitFrame with kernellock==0");
            break;
          case CheckpointFrame:
          case PDRFrame:
          case DiskDirFrame:
            break;
          default:
            crash("check576 Invalid cte->ctefmt found");
         } /* end switch (cte->ctefmt) */
         if (cte->corelock==0x80)
/* sampled  2 .... */
            crash("CHECK106 Page on freelist not free");
/*
         END CHECK_A_PAGE
*/
      } /* End for each Core Table Entry */
   } /* END CHECK_ALL_PAGES */
 
 
/*
 ======================================================================
 
  Now see that all prepared keys are on chains once and only once
*/
   checkslots();
 
   check_memory_map(); /* machine-dependent check of mapping */
/*
*======================================================================
*
* BEGIN CALCULATE_TIME
         LM R2,R3,SAVEPT
         STPT SAVEPT
         LM R0,R1,SAVEPT      TIME AT END
         STM R2,R3,SAVEPT
         SLRDBL R2$R3,R0$R1  DOUBLE SUBTRACT
         STM R2,R3,TIMLSTCK   TIME WE SPENT IN THIS CALL TO CHECK
         ALDBL R2$R3,TIMTOTCK     DOUBLE ADD, ACCUMULATE TOTAL
         STM R2,R3,TIMTOTCK   TOTAL TIME SPENT IN CHECK
* END CALCULATE_TIME
*/
/*
 
  END CHECK
 
 ======================================================================
 ======================================================================
*/
} /* End check */
 
void checkrunning(void)  /* Run check when we may be running a domain */
{
   if (cpudibp && cpudibp != &idledib) {
      char lock;
      bool stopped = stop_process_timer();
/* Don't charge the innocent user for this time! */
      if (!(cpudibp->rootnode->preplock & 0x80) )
         crash("CHECK118 cpudibp domain not preplocked");
      lock = cpudibp->rootnode->preplock;
      cpudibp->rootnode->preplock = 3;
      check();
      cpudibp->rootnode->preplock = lock;
      if (stopped) start_process_timer();
   }
   else check();
}

#include "unprndh.h"
//This routine is called only in kernel testing (provocation) phases.
void trouble(void)
{  //  This is code to stir up "trouble" and reveal a class of bugs.
     static int slotCount = 0;
     static int nodeCount = 0;
     static int tried = 0;
     static int failed = 0;
     NODE * np = (firstnode + nodeCount);
     if (tzcda((short*)&np->cda) && np->prepcode == prepasdomain) {
       struct Key * kp = &(np -> keys[15 & ++slotCount]);
       if((kp -> type & (involvedr | involvedw))
           && (kp -> type != pihk)) {
         ++tried;
         failed += superzap(kp);}}
     if ((nodeCount += 11) >= numberofnodes) nodeCount -= numberofnodes;
}
