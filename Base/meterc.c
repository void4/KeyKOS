/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "kktypes.h"
#include "keyh.h"
#include "cpujumph.h"
#include "locksh.h"
#include "timemdh.h"
#include "gateh.h"
#include "prepkeyh.h"
#include "kschedh.h"
#include "unprndh.h"
#include "wsh.h"
#include "memomdh.h"
#include "domamdh.h"
#include "domainh.h"
#include "scsh.h"
#include "meterh.h"
#include "memutil.h"

typedef uint64* metp;
/* This new type is used exactly in those places where there is code
that knows that CPU counts in meters are stored in the left most
64 bits of key slot 3 of the meter. 
Should we change the form of data keys these spots will most likely
require modifications */
 
unsigned long idlrenc = 0;
   /* Number of renewals of idledib.cpucache */
 
static NODE *hooktonode(
   union Item *ip)
/* Returns ptr to the node containing the hook at *ip */
{
   register NODE *np;
      /* Hook key is always in same slot, subtract gives node header */
             /* Initial value of np below doesn't matter. */
   return((NODE *) ((char *)ip-((char *)&np->domhookkey-(char *)np)));
}
 
static NODE *metertonode(
   union Item *ip)
/* Returns ptr to the node whose dommeterkey
          is the (meter) key at *ip */
{
   register NODE *np;
             /* Initial value of np below doesn't matter. */
   return((NODE *) ((char *)ip-((char *)&np->dommeterkey-(char *)np)));
}

void retcache(struct DIB *dib)    /* Return cache(s) from the dib to the meters */
{
   struct Key *mk = &dib->rootnode->dommeterkey;
   unsigned long uc = stealcache(dib);
   while(mk->type & prepared) {
           *(metp)&((NODE *)mk->nontypedata.ik.item.pk.subject)->keys[3] += uc;
           mk = &((NODE *)mk->nontypedata.ik.item.pk.subject)->keys[1];
   }
} /* End retcache */

struct domcache scavenge_meter(NODE *node, int vigor)
/* recursive function to scavenge a meter */
/* vigor is 0, 1 or 2 depending on which of
  three levels of side-effects are required.
  For vigor=0, leave meters prepared, cpu counts involved,
  and reclaim aged caches and age unaged caches.
  For vigor=1, reclaim all caches.
  For vigor=2, reclaim caches and unprepare meters. */
/* Returns the ammount of cache recovered. */
{
   struct domcache accum;
   unsigned char dry = 0xe0;
   register union Item *k;
   union Item *nextk;

#if defined(viking)
   accum.dom_instructions = 0;
   accum.dom_cycles       = 0;
   accum.ker_instructions = 0;
   accum.ker_cycles       = 0;
   ..foo
#endif
   accum.cputime = 0;

   /* Search chain of all involved keys to this node */
   for (k = node->rightchain;
        k != (union Item *)node; /* exit if no more prepared keys */
        k = nextk) {
      if ((k->key.type & (involvedr+involvedw)) == 0) {
         /* no more involved keys */
         if (k->key.type == resumekey+prepared
                                  /* ugh, there is a resume key! */
             && vigor == 2  /* we uninvolved keys */ ) {
            /* We must put the (newly) uninvolved keys to the right
               of resume keys.
               Put them to the right of all keys.
               k has pointer to first resume key. */
            union Item *p = node->rightchain, *q = node->leftchain;
            /* First remove header from list. */
            q->node.rightchain = p;
            p->node.leftchain  = q;
            /* Now chain header back in just to left of resume keys */
            p = k->key.nontypedata.ik.item.pk.leftchain;
            p->node.rightchain = (union Item *)node;
            node->leftchain = p;
            k->key.nontypedata.ik.item.pk.leftchain =
                                        (union Item *)node;
            node->rightchain = k;
         }
   break;
      }
 
      if (k->key.type == pihk) { /* a hook */
         /* There was an entry key to this meter node! */
         nextk = k->key.nontypedata.ik.item.pk.rightchain;
         rundom(hooktonode(k)); /* foist it off on the cpu queue */
      }
      else if (k->key.type != meterkey + prepared + involvedw)
              crash("METERC000 unknown involved key");
      else { /* a meter key */
         /* Check the preparation of the node which holds this
            meter key. */
         NODE *node2;
         struct domcache temp;
         switch ((node2 = metertonode(k))->prepcode) {
         case prepasdomain: {
            struct DIB *dibp = node2->pf.dib;
            if (!vigor && !(dibp->readiness & STALECACHE)) {
                  /* spare it this time */
                  dibp->readiness |= STALECACHE;
                  dry = 0;
            break;
            }
            dibp->readiness &= ~STALECACHE;
            accum.cputime += stealcache(dibp);  /* move cache to accum */
#if defined(viking)
            accum.dom_instructions += dibp->dom_instructions;
            accum.dom_cycles       += dibp->dom_cycles;
            accum.ker_instructions += dibp->ker_instructions;
            accum.ker_cycles       += dibp->ker_cycles;
            dibp->dom_instructions = 0;
            dibp->dom_cycles       = 0;
            dibp->ker_instructions = 0;
            dibp->ker_cycles       = 0;
 ..foo
#endif
            break;
         }
         case prepasmeter:
            if (!vigor && node2->pf.drys & 0x80)
         break;    /* no caches here, don't bother to scan lower */
            temp = scavenge_meter(node2, vigor);
            dry &= node2->pf.drys;
            accum.cputime += temp.cputime;
#if defined(viking)
            accum.dom_instructions += temp.dom_instructions;
            accum.dom_cycles       += temp.dom_cycles;
            accum.ker_instructions += temp.ker_instructions;
            accum.ker_cycles       += temp.ker_cycles;
..foo
#endif
            break;
         default:
             crash("METERC001 unknown prepcode holding meter key");
         } /* end of switch on prepcode */
         if (vigor == 2) k->key.type &= ~involvedw;
         nextk = k->key.nontypedata.ik.item.pk.rightchain;
      }
   } /* end of loop over all involved keys to this node */
   /* restore cache to resource counter */
   *(metp)&node->keys[3].nontypedata.dk11.fill1 += accum.cputime;
#if defined(viking)
   *(long long *)&node->keys[6].nontypedata.dk11.databody11[3] 
            += accum.dom_instructions;
   *(long long *)&node->keys[10].nontypedata.dk11.databody11[3] 
            += accum.dom_cycles;
   *(long long *)&node->keys[11].nontypedata.dk11.databody11[3] 
            += accum.ker_instructions;
   *(long long *)&node->keys[12].nontypedata.dk11.databody11[3] 
            += accum.ker_cycles;
 ..foo
#endif
   if (vigor == 2) { /* unpreparing */
      node->keys[0].type &= ~involvedw;
      node->keys[3].type = datakey;
      { union {uint64 l; struct{uint32 hi; uint32 lo;} d;} U;
        struct Key * cpc = &node->keys[3];
        U.l = *(metp)&cpc->nontypedata.dk11.fill1;
        *(uint32*)&cpc->nontypedata.dk11.fill1 = 
             *(uint32*)&cpc->nontypedata.dk11.databody11[7];
        *(uint32*)&cpc->nontypedata.dk11.databody11[3] = U.d.hi;
        *(uint32*)&cpc->nontypedata.dk11.databody11[7] = U.d.lo;}
      node->keys[4].type = datakey;
      node->keys[5].type = datakey;
      node->keys[6].type = datakey;
      node->keys[8].type &= ~involvedw;
      node->keys[10].type = datakey;
      node->keys[11].type = datakey;
      node->keys[12].type = datakey;
      node->keys[15].type &= ~involvedw;
      if (node->keys[7].type == chargesetkey+involvedw)
         scsuninv(&(node->keys[7]));
      else
         node->keys[7].type &= ~involvedw;
      node->dommeterkey.type &= ~involvedw;
         /* don't need to change the order of this key on the
            backchain, because we are going to uninvolve everything */
      node->prepcode = unpreparednode;
   }
   node->pf.drys = dry;  // Record observed dryness.
   return accum;
} /* end of scavenge_meter */
 
static void restore_cache(
   register NODE *node,
   struct domcache *acache)
/* restore meter cache to all higher meters */
{
   while (node->dommeterkey.type & prepared
              /* until we reach the super meter */  ) {
      node = (NODE *)
             node->dommeterkey.nontypedata.ik.item.pk.subject;
      if (node->prepcode != prepasmeter)
         crash("METERC002 prepared meter has unprepared superior");
      /* Restore cache */
      *(metp)&node->keys[3].nontypedata.dk11.fill1 += acache->cputime;
#if defined(viking)
      *(long long *)&node->keys[6].nontypedata.dk11.databody11[3] 
               += acache->dom_instructions;
      *(long long *)&node->keys[10].nontypedata.dk11.databody11[3] 
               += acache->dom_cycles;
      *(long long *)&node->keys[11].nontypedata.dk11.databody11[3] 
               += acache->ker_instructions;
      *(long long *)&node->keys[12].nontypedata.dk11.databody11[3] 
               += acache->ker_cycles;
..foo
#endif
   }
}
 
void unprmet(node)        /* Unprepare a prepared meter node */
/* Input - */
NODE *node;
{
   struct domcache cache;
 
   if ((node->dommeterkey.type & involvedw) == 0)
       crash("METERC003 meter key not involved");
   cache = scavenge_meter(node, 2);
   /* Adjust the backchain order of the meter key we just
             uninvolved. */
   if (node->dommeterkey.type & prepared
          /* it isn't the super meter key */  )
      uninvolve(&(node->dommeterkey));
   restore_cache(node, &cache);
} /* End unprmet */
 
 
static void rechain_as_involved(
   register struct Key *key)
/* Involve a prepared key and move it to the involved position
   in the backchain. */
{
   register union Item *p = key->nontypedata.ik.item.pk.leftchain,
    *q = key->nontypedata.ik.item.pk.rightchain;
 
   /* Unchain the key from the backchain */
   ((NODE *)p)->rightchain = q;
   ((NODE *)q)->leftchain = p;
    /* Chain it into the involved position in the backchain */
   p = key->nontypedata.ik.item.pk.subject;
   key->nontypedata.ik.item.pk.leftchain = p;
   q = ((NODE *)p)->rightchain;
   key->nontypedata.ik.item.pk.rightchain = q;
   ((NODE *)q)->leftchain = ((NODE *)p)->rightchain = (union Item *)key;
   key->type |= involvedw;
}
 
static char sixzeroes[6] = {0,0,0,0,0,0};
 
static int ensure_involved_meterkey(
   struct Key *key,
   int depth)       /* maximum depth to go */
/* Recursive routine to ensure a key is an involved meter key */
/* Returned value is:
   <=0: successful. key is now an involved meter key.
       It is either the super meter key, or an involved prepared
       meter key to a node prepared as a meter.
       The returned value is the negative of the unused depth.
   1:  Actor enqueued.
   >1: a value for the trap code
 */
{
   NODE *node;
   int rc;
 
   if (key->type != meterkey+prepared) { /* not a prepared meter key */
      if (key->type != meterkey) /* not a meter key at all */
         return 0x400+4;  // 0x404 for grep
      if (Memcmp(key->nontypedata.ik.item.upk.cda,
                 sixzeroes,
                 6 ) == 0) { /* the super meter key */
         key->type |= involvedw;   /* involve it */
         return (-depth);    /* return unused depth */
      }
      switch(prepkey(key)) {
         case prepkey_notobj: /* key was obsolete */
            return 0x400+4; // 0x404 for grep
         case prepkey_prepared:
            break;
         case prepkey_wait:
            return 1;
      }
   }
 
   /* key is a prepared meter key */
   node = (NODE *)key->nontypedata.ik.item.pk.subject;
   if (node->prepcode != prepasmeter) {
      int tdk0(struct Key * kp){
         uchar * ip = kp -> nontypedata.dk11.databody11;
         return kp -> type | *(char*)ip | *(short*)(ip+1) | *(int*)(ip+3);}
      if (depth == 0) /* we can't go any deeper */
         return 0x400+16;  /* meter tree is too deep */
      if (node->prepcode != unpreparednode)
         if (unprnode(node) != unprnode_unprepared)
            return 0x400+12;    /* can't unprepare */
      /* node is now unprepared */
      if (preplock_node(node, lockedby_prepmeter))
         return 0x400+12;   /* already preplocked */
      /* Check that it is a valid meter */
      if (Memcmp(node->keys[3].nontypedata.dk11.databody11,
                 dk0.nontypedata.dk11.databody11,
                 3)       /* slot 3 must be dk(not too big) */
          | tdk0(&node->keys[0])      /* slot 0 must be dk(0) */
          | node->keys[3].type
          | node->keys[4].type
          | node->keys[5].type
          | node->keys[6].type
          | node->keys[8].type
          | node->keys[10].type
          | node->keys[11].type
          | node->keys[12].type
          | node->keys[15].type) {
         unpreplock_node(node);
         return 0x400+8;   /* node is not a valid meter */
      }
      if (node->keys[7].type == chargesetkey) {
         scsinvky(&node->keys[7]);
      }
      else {  /* slot 7 is not a chargeset key */
         /* then must be dk(0) */
         if (tdk0(&node->keys[7])) {
            unpreplock_node(node);
            return 0x400+8;
         }
      }
      /* This node is correctly formatted for a meter node. */
      /* Try to involve its slot 1 recursively. */
      rc = ensure_involved_meterkey(&node->keys[1], depth-1);
      if (rc > 0) { /* some problem */
         if (node->keys[7].type == chargesetkey+involvedw)
            scsuninv(&node->keys[7]); /* uninvolve it */
         unpreplock_node(node);
         return rc;
      }
      /* Finish the preparation of the meter node. */
      node->keys[0].type |= involvedw;
      node->keys[3].type |= involvedw+involvedr;
      node->keys[4].type |= involvedw+involvedr;
      node->keys[5].type |= involvedw+involvedr;
      node->keys[6].type |= involvedw+involvedr;
      if (node->keys[7].type != datakey)
         scsunlk(&node->keys[7]);   /* unlock the charge set */
      else node->keys[7].type |= involvedw; /* involve the data key */
      node->keys[8].type |= involvedw;
      node->keys[10].type |= involvedw+involvedr;
      node->keys[11].type |= involvedw+involvedr;
      node->keys[12].type |= involvedw+involvedr;
      node->keys[15].type |= involvedw;
      node->meterlevel = depth + rc;  /* set level of this meter */
      node->pf.drys = 0xe0;  /* caches initially dry */
      node->flags |= NFDIRTY;
      { union {uint64 l; struct{uint32 hi; uint32 lo;} d;} U;
        struct Key * cpc = &node->keys[3];
        U.d.hi = *(uint32*)&cpc->nontypedata.dk11.databody11[3];
        U.d.lo = *(uint32*)&cpc->nontypedata.dk11.databody11[7];
        *(uint32*)&cpc->nontypedata.dk11.databody11[7] =
                  *(uint32*)&cpc->nontypedata.dk11.fill1;
        *(metp)&cpc->nontypedata.dk11.fill1 = U.l;}
      node->prepcode = prepasmeter;
      rechain_as_involved(key);  /* Involve the key */
      unpreplock_node(node);
      return rc;
   } /* end if node is not prepared as a meter */
   /* Node was prepared as meter already. */
   if ((rc = node->meterlevel - depth) > 0) /* meter is too deep */
      return 0x400+16;
   /* We found a prepared meter at an acceptable depth. */
   rechain_as_involved(key);  /* Involve the key */
   return rc;      /* Return with success indication (rc <= 0) */
} /* end of ensure_involved_meterkey */
 
static csid chargeset_id;
static NODE *dry_meter;
static unsigned long borrow_cache(
   struct Key *key,
   unsigned long amount) /* amount of cache to try to get */
/* Recursive routine to borrow a cache of CPU time from a meter key */
/* Returns the amount of cache gotten.
   If amount returned is zero, dry_meter is set to the meter that
      had no resources.
   Otherwise, chargeset_id is set to the nearest chargeset id.
 */
{
   register NODE *node;
   uint64 value;
 
   if ((key->type & prepared) == 0) { /* reached the super meter */
      chargeset_id = 0;  /* super meter has no chargeset */
      return amount;     /* can get the whole amount */
   }
   node = (NODE *)key->nontypedata.ik.item.pk.subject;
   if (node->prepcode != prepasmeter)
      crash("METERCxxx meter key to node not prepasmeter");
   if ((node->dommeterkey.type & involvedw) == 0)
       crash("METERC004 meter key not involved");
   value = amount;
   /* Is there enough cache available in this meter? */ 
   if(*(metp)&node->keys[3].nontypedata.dk11.fill1 < value)  {
      /* Not enough. Must scavenge or settle for less or
         jump to keeper. */
      if (!*(metp)&node->keys[3].nontypedata.dk11.fill1){
         /* no cache left in the meter */
         /* Must scavenge or jump to keeper. */
         /* If the meter is really dry (we have nothing to scavenge),
            then the bit in drys will be set. */
         if (node->pf.drys & 0x80) { /* no cache available */
            dry_meter = node;
            return 0;
         }
         /* Go get the caches from all the DIBs under this meter
            that have them. */
         {
            struct domcache cache = scavenge_meter(node, 0);
            restore_cache(node, &cache);
         }
         /* Now look again. */
         if(!*(metp)&node->keys[3].nontypedata.dk11.fill1) { /* no cache left in the meter */
                                                             /* The meter should now be really dry. */
            {  /* try harder */
                struct domcache cache = scavenge_meter(node, 0);
                restore_cache(node, &cache);
            }
            if(!*(metp)&node->keys[3].nontypedata.dk11.fill1) { /* truely dry best be marked so */
                if ((node->pf.drys & 0x80) == 0) crash("METERCxxx scavenged meter isn't dry");
                /* no cache available */
                dry_meter = node;
                return 0;
            }
         }
      } /* end of no cache left (before scavenging) */
      /* Settle for less. */
      /* copy the available cache to value */
     value = *(metp)&node->keys[3].nontypedata.dk11.fill1;
   }
   /* There is enough cache in this meter. */
   if ((value = borrow_cache(&node->keys[1], value)) == 0)
      return 0;  /* we didn't get any from higher up */
   /* Subtract the cache we are getting from this meter. */
   *(metp)&node->keys[3].nontypedata.dk11.fill1 -= value;
   node->pf.drys &= ~0x80;  /* there is a cache now */
   if (node->keys[7].type == chargesetkey+involvedw)
      chargeset_id = scsgetid(&node->keys[7]);
   return value;
} /* end of borrow_cache */
 
void refill_cpucache(void)
/* Attempt to refill cpudibp->cpucache which is zero. */
/* Prepares meters as necessary */
// May result in setting cpudibp to 0
// or setting it to locate meter keeper.
{
   if (cpudibp->cpucache)
      crash("METERCxxx refilling nonempty cache");
   if (cpudibp == &idledib) {
      idlrenc += 1;  /* count number of refills */
      /* Install new cache in cpudibp */
      cpudibp->cpucache = 0x7fffffff;  /* use maximum cache */
      cpudibp->readiness &= ~ZEROCACHE;
      loadpt();
   }
   else { /* refilling a normal domain */
      unsigned long cache_gotten;
 
      /* Begin ensure meter key is involved */
      if ((cpudibp->rootnode->dommeterkey.type & involvedw) == 0) {
         int rc = ensure_involved_meterkey(&cpudibp->rootnode->dommeterkey,
                                       20  /* max depth */ ) ;
         if (rc > 0 ) {    /* some problem */
            if (rc != 1) set_trapcode(cpudibp, rc); // Crud in the meters
            putawaydomain();
            return;
         }
      }
      else { /* meter key in domain root is involved */
         if (cpudibp->rootnode->dommeterkey.type !=
                   meterkey+prepared+involvedw
             && (cpudibp->rootnode->dommeterkey.type !=
                   meterkey+involvedw
                 || Memcmp(
            cpudibp->rootnode->dommeterkey.nontypedata.ik.item.upk.cda,
                           sixzeroes,
                           6) != 0 ) )
            crash("METERCxxx strange involved key as domain meter");
      }
      /* End ensure meter key is involved */
 
      if ((cache_gotten =
               borrow_cache(&cpudibp->rootnode->dommeterkey,
                            5000000*16 /* 5 seconds */ ) )
           == 0) { /* couldn't get any cache */

         uint64 timer;

         /* invoke the meter keeper */
         /* third key passed is node key to meter node */
         cpup3key.databyte = 0;
         cpup3key.type = nodekey+prepared;
         cpup3node = dry_meter;
         cpuordercode = -1;
         cpuexitblock.keymask = 2;
         cpuexitblock.argtype = arg_regs;
         cpuarglength = 8;
         timer = read_system_timer();
         cpuargaddr = (char *)&timer;
 
         keepjump(&dry_meter->keys[2], restartresume);
      }
      else { /* got some cache */
         if (cpudibp->chargesetid != chargeset_id) {
            /* The chargeset id has changed. */
            cpudibp->chargesetid = chargeset_id;
            zap_dib_map(cpudibp); /* Force "memory" to look at it */
         }
         /* Install new cache in cpudibp */
         cpudibp->cpucache = cache_gotten;
         cpudibp->readiness &= ~ZEROCACHE;
         loadpt();
      }
   } /* end of refilling a normal domain */
}
