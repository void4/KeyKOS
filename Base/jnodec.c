/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "keyh.h"
#include "cpujumph.h"
#include "gateh.h"
#include "locksh.h"
#include "wsh.h"
#include "prepkeyh.h"
#include "primcomh.h"
#include "nodedefs.h"
#include "domamdh.h"
#include "kernkeyh.h"
#include "jnodeh.h"
#include "meterh.h"
#include "memutil.h"

 
int nodeslowtest(   /* See if we have to do a slow call */
   register NODE *node)
{
   if (cpuexitblock.jumptype != jump_call
       || (node->prepcode != unpreparednode
           && (node == cpudibp->rootnode
               || node == cpudibp->keysnode
               || node_overlaps_statestore(node) )))
      return 1;
   return 0;
}
 
 
void clearnode(      /* Clear a node to DK0 */
   register NODE *node)
{
   register struct Key *key;
 
   node->flags |= NFDIRTY;
   for (key = node->keys; key <= node->keys+15; key++) {
      if (puninv(key)) crash("JNODE001 Can't unprepare node");
      if (clean(key) == clean_ok) *key = dk0;
   }
}
 
 
static int prep_slots(     /* Prepare slots for write */
   register NODE *node,               /* The node to write into */
   register unsigned long first,
   register unsigned long last) /* The first and last slots */
/*
   Output -
      Returns 1 if another preplocked node prevented unpreparing node
      Returns 0 if all slots can be written.
*/
{
   for (;;) {
      if (puninv(node->keys + first))
return 1;
      if (first == last)
return 0;
      if (++first == 16) first = 0;
   }
}
 
static inline void fastNodeFetch() { 
    cpuordercode = 0;
    cpuarglength = 0;
    cpuexitblock.keymask = 8;
    cpup1key.type &= ~involvedw;
    jsimplecall();
    zapresumes(cpudibp);
}

static void slowNodeFetch(register NODE *node) { 
    corelock_node(10, node);   /* Core lock the node */
    switch (ensurereturnee(0)) {
    case ensurereturnee_wait:  {
	coreunlock_node(node);
	abandonj();
	return;
    }
    case ensurereturnee_overlap: {
	coreunlock_node(node);
	midfault();
	return;
    }
    case ensurereturnee_setup: handlejumper();
    }
    /* End dry run */
    cpup1key = *readkey(node->keys + cpuordercode-NODE__FETCH);
    cpup1key.type &= ~involvedw;
    coreunlock_node(node);
    cpuordercode = 0;
    cpuarglength = 0;
    cpuexitblock.keymask = 8;
    if (! getreturnee()) { return_message(); }
}

void jnode1(register NODE *node) {   /* Handle jumps to node keys */
    if(cpuordercode < 16) {
	struct Key *s = node->keys + cpuordercode-NODE__FETCH;
	if (cpuexitblock.jumptype != jump_call) {
	    slowNodeFetch(node); 
	} else if (! (s->type & involvedr)) {
	    cpup1key = *s;
	    fastNodeFetch();
	} else if (node != cpudibp->rootnode 
		   && (s = readkey(s)) != NULL) {
	    cpup1key = *s;
	    fastNodeFetch();
	} else if (node -> prepcode == prepasmeter && cpuordercode == 3) {
	    scavenge_meter(node, 1);
	    *(uint32*)&cpup1key = *((uint32*)s+2);
	    *((uint32*)&cpup1key+1) = *((uint32*)s);
	    *((uint32*)&cpup1key+2) = *((uint32*)s+1);
	    *((uint32*)&cpup1key+3) = 0;
	    fastNodeFetch();
	} else {
	    slowNodeFetch(node);
	}
    } else { // node swap keys
         register struct Key *s = node->keys + cpuordercode-NODE__SWAP;
         register struct Key *k = ld1();
 
         if ( s->type & involvedw      /* No big deal if not involvedw */
              && !(s->type & involvedr) /* Can't compare if involvedr */
              && compare_keys(s, k) == 0) { /* Same - don't undo involvement */
            cpup1key = *k;
            cpuordercode = 0;
            cpuarglength = 0;
            jsimple(8);
            zapresumes(cpudibp);
            return;
         }
         if (cpuexitblock.jumptype != jump_call ||
               node == cpudibp->rootnode ||
               puninv(s)) {       /* Swap the slow way */
 
            corelock_node(11, node);   /* Core lock the node */
            switch (ensurereturnee(0)) {
             case ensurereturnee_wait:  {
               coreunlock_node(node);
               abandonj();
               return;
             }
             case ensurereturnee_overlap: {
               coreunlock_node(node);
               midfault();
               return;
             }
             case ensurereturnee_setup: ;
            }
            k = prep_passed_key1();
            if (k == NULL) {
               coreunlock_node(node);
               unsetupreturnee();
               abandonj();
               return;
            }
            s = node->keys + cpuordercode-NODE__SWAP;
            if (puninv(s)) crash("JNODE002 can't unprepare node");
            handlejumper();
         /* End dry run */
            cpup1key = *look(s);
            if (clean(s) == clean_ok) {
               *s = *k;
               if (s->type & prepared) halfprep(s);
               node->flags |= NFDIRTY;
            }
            coreunlock_node(node);
            cpuordercode = 0;
            cpuarglength = 0;
            cpuexitblock.keymask = 8;
            if (! getreturnee()) return_message();
            return;
 
         } /* End swap the slow way */
         else {                 /* Swap the fast way */
/*
            Now we are getting down to the basic simple stuff.
 
            WE CAN DO IT THE FAST WAY.
 
*/
            cpup1key = *look(s);
            if (clean(s) == clean_ok) {
               *s = *k;
               if (s->type & prepared) halfprep(s);
               node->flags |= NFDIRTY;
            }
            cpuordercode = 0;
            cpuarglength = 0;
            cpuexitblock.keymask = 8;
            jsimplecall();
            zapresumes(cpudibp);
            return;
         } /* End swap the fast way */
      }  // node swap keys
} /* End jnode1 */
 
 
void jfetch(key)   /* Handle jumps to fetch keys */
struct Key *key;
{
   if (cpuordercode < 16)
      jnode1((NODE *)key->nontypedata.ik.item.pk.subject);
   else if (cpuordercode == NODE__DATA_BYTE) {
      simplest(key->databyte);
      return;
   }
   else if (cpuordercode == KT) simplest(4);
   else simplest(KT+2);
} /* End jfetch */
 
 
void jnode(key)    /* Handle jumps to node keys */
struct Key *key;
{
   register NODE *node = (NODE *)key->nontypedata.ik.item.pk.subject;

   if (cpuordercode <  32) jnode1(node);
     
   else switch (cpuordercode) {
    case NODE__COMPARE:
      {
         register struct Key *s = ld1();
         register unsigned char type;
/*
         N.B. tryprep, in ld1, will have prepared the caller's key if
              it is to "node" since "node" is already in node space
*/
         if (s->type & prepared &&
             (NODE *)s->nontypedata.ik.item.pk.subject == node &&
               (type = s->type & keytypemask,
                type == fetchkey || type == nodekey ||
                type == segmentkey || type == meterkey ||
                type == sensekey || type == frontendkey))
            cpuordercode = 0;
         else cpuordercode = 1;
         cpuarglength = 0;
         jsimple(8);  /* first key */
         return;
      }
 
    case NODE__CLEAR:
      if (nodeslowtest(node)) {  /* Clear the slow way */
         corelock_node(12, node);   /* Core lock the node */
         switch (ensurereturnee(0)) {
          case ensurereturnee_wait:
            coreunlock_node(node);
            abandonj();
            return;
          case ensurereturnee_overlap:
            coreunlock_node(node);
            midfault();
            return;
          case ensurereturnee_setup: break;
         }
         handlejumper();
         clearnode(node);
         coreunlock_node(node);
         cpuordercode = 0;
         cpuarglength = 0;
         cpuexitblock.keymask = 0;
         if (! getreturnee()) return_message();
         return;
 
      } /* End clear the slow way */
      else {                 /* Clear the fast way */
         clearnode(node);
         cpuordercode = 0;
         cpuarglength = 0;
         cpuexitblock.keymask = 0;
         jsimplecall();
         return;
      } /* End clear the fast way */
 
    case NODE__DATA_BYTE:
      simplest(key->databyte);
      return;
 
    case NODE__MAKE_FETCH_KEY:
      cpup1key = *key;
      cpup1key.type = fetchkey | (cpup1key.type & prepared);
      pad_move_arg(&cpup1key.databyte, 1);
      cpuordercode = 0;
      cpuarglength = 0;
      jsimple(8);  /* First key */
      return;
 
    case NODE__MAKE_SEGMENT_KEY:
      cpup1key = *key;
      cpup1key.type = segmentkey | (cpup1key.type & prepared);
      pad_move_arg(&cpup1key.databyte, 1);
      cpuordercode = 0;
      cpuarglength = 0;
      jsimple(8);  /* first key */
      return;

    case NODE__MAKE_FRONTEND_KEY:
      cpup1key = *key;
      cpup1key.type = frontendkey | (cpup1key.type & prepared);
      pad_move_arg(&cpup1key.databyte, 1);
      cpuordercode = 0;
      cpuarglength = 0;
      jsimple(8);  /* first key */
      return;
 
    case NODE__MAKE_METER_KEY:
      cpup1key = *key;
      cpup1key.type = meterkey | (cpup1key.type & prepared);
      cpup1key.databyte = 0;
      cpuordercode = 0;
      cpuarglength = 0;
      jsimple(8);   /* first key */
      return;
 
    case NODE__MAKE_NODE_KEY:
      cpup1key = *key;
      /* Key type is already nodekey */
      pad_move_arg(&cpup1key.databyte, 1);
      cpuordercode = 0;
      cpuarglength = 0;
      jsimple(8);   /* first key */
      return;

    case NODE__MAKE_SENSE_KEY:
      cpup1key = *key;
      cpup1key.type = sensekey | (cpup1key.type & prepared);
      cpup1key.databyte |= (readonly | nocall);
      cpuordercode=0;
      cpuarglength = 0;
      jsimple(8);
      return;
 
    case NODE__WRITE_DATA:
      {
         unsigned long word;        /* N.B. Can't be made register */
         register unsigned long first, last;
         register struct Key *s;    /* The slot to store */
         char str[16];
 
         pad_move_arg((char *)&word,4);
         first = word;
         pad_move_arg((char *)&word,4);
         last = word;
         if (first > 15 || last > 15) {
            simplest(1);
            return;
         }
         cpuordercode = 0;
         if (cpuexitblock.jumptype != jump_call ||
               node == cpudibp->rootnode ||
               prep_slots(node,first,last)) { /* Write data slow way */
            corelock_node(13, node);   /* Core lock the node */
            switch (ensurereturnee(0)) {
             case ensurereturnee_wait:  {
                coreunlock_node(node);
                abandonj();
                return;
             }
             case ensurereturnee_overlap: {
                coreunlock_node(node);
                midfault();
                return;
             }
             case ensurereturnee_setup: handlejumper();
            }
            /* End dry run */
            node->flags |= NFDIRTY;
            for (s = node->keys + first;
                 ;
                 s = node->keys + first) {
               if (puninv(s)) crash("JNODE003 Can't unprepare node");
               pad_move_arg(str,16);      /* Get next data */
               if (Memcmp(str,"\0\0\0\0\0",5)) cpuordercode = 2;
               if (clean(s) == clean_ok) {
                  Memcpy(s->nontypedata.dk11.databody11,str+5,11);
                  s->type = datakey;
               }
               if (first == last)
            break;
               if (++first == 16) first = 0;
            }
            coreunlock_node(node);
            cpuarglength = 0;
            cpuexitblock.keymask = 0;
            if (! getreturnee()) return_message();
            return;
 
         } /* End write data slow way */
         else {                 /* Write data the fast way */
/*
            Now we are getting down to the basic simple stuff.
 
            WE CAN DO IT THE FAST WAY.
 
*/
            node->flags |= NFDIRTY;
            for (s = node->keys + first;
                 ;
                 s = node->keys + first) {
               pad_move_arg(str,16);      /* Get next data */
               if (Memcmp(str,"\0\0\0\0\0",5)) cpuordercode = 2;
               if (clean(s) == clean_ok) {
                  Memcpy(s->nontypedata.dk11.databody11,str+5,11);
                  s->type = datakey;
               }
               if (first == last)
            break;
               if (++first == 16) first = 0;
            }
            cpuarglength = 0;
            cpuexitblock.keymask = 0;
            jsimplecall();
            return;
         } /* End write data the fast way */
      }
    default:
      if (cpuordercode == KT) simplest(3);
      else simplest(KT+2);
      return;
   } /* End switch on cpuordercode */
} /* End jnode */
