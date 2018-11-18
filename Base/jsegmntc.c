/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include "string.h"
#include "sysdefs.h"
#include "keyh.h"
#include "cpujumph.h"
#include "gateh.h"
#include "geteh.h"
#include "locksh.h"
#include "prepkeyh.h"
#include "primcomh.h"
#include "memoryh.h"
#include "cpumemh.h"
#include "queuesh.h"
#include "wsh.h"
#include "kernkeyh.h"
#include "nodedefs.h"
#include "jsegmenth.h"
#include "locore.h"
#include "memutil.h"

/* #include "snode.h" */
#define SNode_Fetch 41
#define SNode_Swap  42
#define SNode_MoveRegs 50

 
 
struct Key *s15;
 
 
static int check_format_key(       /* Check the format key */
   register struct Key *key)              /* The segment key */
#define format_red 0     /* Good red segment, s15 is set */
#define format_black 1   /* Seg is black, NoCall or format key is bad */
#define format_overlap 2 /* Jumper and segment overlap */
{
   NODE *seg = (NODE *)key->nontypedata.ik.item.pk.subject;
 
   if (key->databyte & (0x0f + nocall)) return format_black;
   s15 = readkey(&seg->keys[15]);
   if (!s15) return format_overlap;
   if ((s15->type & ~involvedw) != datakey ||     /* Not data key */
        (s15->nontypedata.dk7.databody[5]&240)==240 ||/* No keep */
        s15->nontypedata.dk7.databody[3] & 0xa0) { /* bad fmt */
      return format_black;
   }
   return format_red;
}
 
 
void pad_copy_arg(     /* Copy argument, pad with zeroes */
   register void *to,           /* To address for move */
   register int len)            /* Length of move */
/*
      N.B. This routine does not update cpuargaddr and cpuarglength. In this
           sense it copies rather than moving the string
*/
{
   int ml = len;                /* The length to move */
   register unsigned long arglength = cpuarglength;
   register char *argaddr = cpuargaddr;
 
   if (arglength < ml) ml = arglength;
   if (cpumempg[0] || cpuexitblock.argtype != arg_memory) {
      Memcpy(to, argaddr, ml);
   } else if ( 0 == movba2va(to, argaddr, ml) ) {
      crash("JSEGMNT001 - Overlap with stack?");
   }
   if (len-ml) memzero((char *)to+ml, len-ml);
}
 

int sn_fetch(struct Key *key, uint32 slot)
{
   int depth = s15->nontypedata.dk7.databody[6] & 0x0f;
   int limit;
   uint32 index = 0;
   NODE *node = (NODE *)key->nontypedata.ik.item.pk.subject;
   struct Key *k;

   if (0 == depth) return 0;

   for (limit=0; limit<15; limit++) {   /* Limit depth of search */
      k = readkey(&node->keys[index]);
      if (!k) return 0;
      if (0 == depth) {                 /* We have the key */
         if (k->type & (involvedr+involvedw) ) return 0;
         cpup1key = *k;
         cpuordercode = 0;
         cpuarglength = 0;
         jsimple(8);  /* first key */
         return 1;
      }
      if (depth > 8) return 0;
      if ( (k->type & ~involvedw) != nodekey+prepared) return 0;
      node = (NODE *)k->nontypedata.ik.item.pk.subject;
      depth = k->databyte;
      index = slot >> (depth*4);
      if (index>15) return 0;           /* Structure doesn't hold key */
      slot -= index << (depth*4);       /* Consume part of address */
   }
   return 0;
}
 

int sn_swap(struct Key *key, uint32 slot)
{
   int depth = s15->nontypedata.dk7.databody[6] & 0x0f;
   int limit;
   uint32 index = 0;
   NODE *node = (NODE *)key->nontypedata.ik.item.pk.subject;
   struct Key *k;

   if (0 == depth || key->databyte & readonly) return 0;

   for (limit=0; limit<15; limit++) {   /* Limit depth of search */
      k = readkey(&node->keys[index]);
      if (!k) return 0;
      if (0 == depth) {                 /* We have the key */
         if (puninv(k) == puninv_cant) return 0;
         cpup1key = *k;
         if (clean(k) == clean_ok) {
            *k = *ld1();
            if (k->type & prepared) halfprep(k);
            node->flags |= NFDIRTY;
         }
         cpuordercode = 0;
         cpuarglength = 0;
         jsimple(8);  /* first key */
         return 1;
      }
      if (depth > 8) return 0;
      if ( (k->type & ~involvedw) != nodekey+prepared) return 0;
      node = (NODE *)k->nontypedata.ik.item.pk.subject;
      depth = k->databyte;
      index = slot >> (depth*4);
      if (index>15) return 0;           /* Structure doesn't hold key */
      slot -= index << (depth*4);       /* Consume part of address */
   }
   return 0;
}
/* 
   returns remaining depth

   if return = -1 then must abandon and repeat
   if return = -2 then found a bad fe segment
   if return = 0  then have gone too far

   assumption that check_format_key has been called for "seg"
   seg is corelocked.
   seg will be uncorelocked by caller

   sets keeper slot, keeper 
*/

static int getfrontendkeeper(seg,keeper,keeper_slot,segmentkeydatabyte,depth)
   NODE **seg;
   struct Key **keeper;
   int *keeper_slot;
   char *segmentkeydatabyte;
   int depth;
{
   NODE *newseg,*oldseg;
   int rc;
   int type;

   if(depth == 0) {        /* too deep  */
      return 0;
   }

   *keeper_slot = s15->nontypedata.dk7.databody[5] >> 4 & 0xf;
      /* If keeper_slot is 15, keeper will be the format key,
         which is a data key, which is treated as DK(0). */
   *keeper = readkey(&(*seg)->keys[*keeper_slot]);
   if (!*keeper) crash("JFRONTEND001 Can't readkey front end keeper");
   *keeper = prx(*keeper);
   if (!*keeper) {
       return -1;    /* signal start over */
   }
   type = (*keeper)->type & keytypemask;

   if(type == frontendkey) {
       *segmentkeydatabyte = (*keeper)->databyte;
       if(check_format_key(*keeper) == format_black) {
          return -2;
       }
       newseg = (NODE *)(*keeper)->nontypedata.ik.item.pk.subject;
       corelock_node(6,newseg);
       oldseg=*seg;   /* save this for unlock after unwinding */
       *seg=newseg;   /* seg will be left at the last node encountered, with a real keeper */
       rc = getfrontendkeeper(&newseg,keeper,keeper_slot,segmentkeydatabyte,depth-1);
       coreunlock_node(oldseg);  /* unlock while unwinding */
       return rc;
   }

   return depth;   /* type is not front end */
                   /* seg is final node, keeper is final keeper,keeper_slot is final slot */
}

void jfrontend(struct Key *key)        /* Front End Key, very much like segment key */
{
   NODE *seg = (NODE *)key->nontypedata.ik.item.pk.subject;
   struct Key *keeper;
   int depth,keeper_slot,local_slot;
   char type;
   char segmentkeydatabyte;

   if(check_format_key(key) == format_black) {
       keyjump(&dk0);
       return;
   }
   /* must find end of front end chain */

   corelock_node(6, seg);

   segmentkeydatabyte = key->databyte;

   depth = getfrontendkeeper(&seg,&keeper,&keeper_slot,&segmentkeydatabyte,20);

   if(depth == 0) {   /* loop in front end or simply too many */
      coreunlock_node(seg);
      cputrapcode = 0x600 + 76;
      midfault();
      return;
   }

   if(depth == -1) {   /* can't continue, must start over */
       coreunlock_node(seg);
       abandonj();
       return;
   }
   if(depth == -2) {   /* came up with a dud, treat as dk0 */
       coreunlock_node(seg);
       keyjump(&dk0);
       return;
   }

   type = keeper->type & keytypemask;


   if(type != startkey) {
       coreunlock_node(seg);
       keyjump(&dk0);
       return;
   }
   if (!(s15->nontypedata.dk7.databody[3] & 0x10)) { /* Pass node */
         cpup3node = seg;
         cpup3key.nontypedata.ik.item.pk.subject = (union Item *)seg;
         cpup3key.type = nodekey + prepared;
         cpup3key.databyte = segmentkeydatabyte;
         local_slot = s15->nontypedata.dk7.databody[4] >> 4 & 0xf;
         if (cpup3switch != CPUP3_UNLOCKED) crash("JSEG p3switch set");
         if (local_slot != 15 && local_slot != keeper_slot) {
            /* There is a local slot for the third key */
            if (puninv(&seg->keys[local_slot]) == puninv_cant) {
               crash("JSEGMENT002 Can't clean local slot");
            }
            cpup3switch = CPUP3_JUMPERKEY + local_slot;
            cpustore3key = *ld3(); /* get jumper's third key */
         }
         else cpup3switch = CPUP3_LOCKED;     /* Node locked above */
         cpuexitblock.keymask |= 2;
    }
    else coreunlock_node(seg);
    keyjump(keeper);
    return;
}

void jsegment(key)         /* Segment key */
   struct Key *key;           /* The segment key invoked */
{
   register NODE *seg = (NODE *)key->nontypedata.ik.item.pk.subject;
   register char segmentkeydatabyte = key->databyte;
   register char type;
   register struct Key *keeper;
   uint32 slot;           /* Slot number for supernode emulation */
 
   switch (check_format_key(key)) {
      int keeper_slot, local_slot;
    case format_red:
      /* 
         Check for kernel supernode handling
      */
      if (s15->nontypedata.dk7.databody[3] & 0x40) {  /* It is a kernel SN */
         switch (cpuordercode) {
	  case NODE__FETCH:
          case NODE__FETCH+1:
          case NODE__FETCH+2:
          case NODE__FETCH+3:
          case NODE__FETCH+4:
          case NODE__FETCH+5:
          case NODE__FETCH+6:
          case NODE__FETCH+7:
          case NODE__FETCH+8:
          case NODE__FETCH+9:
          case NODE__FETCH+10:
          case NODE__FETCH+11:
          case NODE__FETCH+12:
          case NODE__FETCH+13:
          case NODE__FETCH+14:
          case NODE__FETCH+15:
            slot = cpuordercode - NODE__FETCH;
            if (sn_fetch(key, slot)) return;
            break;
          case NODE__SWAP:
          case NODE__SWAP+1:
          case NODE__SWAP+2:
          case NODE__SWAP+3:
          case NODE__SWAP+4:
          case NODE__SWAP+5:
          case NODE__SWAP+6:
          case NODE__SWAP+7:
          case NODE__SWAP+8:
          case NODE__SWAP+9:
          case NODE__SWAP+10:
          case NODE__SWAP+11:
          case NODE__SWAP+12:
          case NODE__SWAP+13:
          case NODE__SWAP+14:
          case NODE__SWAP+15:
            slot = cpuordercode - NODE__SWAP;
            if (sn_swap(key, slot)) return;
            break;
          case SNode_Fetch:
            pad_copy_arg(&slot, sizeof(slot));
            if (sn_fetch(key, slot)) return;
            break;
          case SNode_Swap:
            pad_copy_arg(&slot, sizeof(slot));
            if (sn_swap(key, slot)) return;
            break;
          case SNode_MoveRegs:
            cpup1key = *ld1();
            cpup2key = *ld2();
            cpup3key = *ld3();  /* May return pointer to cpup3key */
            cpuordercode = 0;
            cpuarglength = 0;
            jsimple(0xe);   /* return 3 keys */
            return;

          default: break;    /* Fall thru to call the keeper */
         }
      }
      /*
         Send jump to segment keeper key
      */
      corelock_node(6, seg);
      keeper_slot = s15->nontypedata.dk7.databody[5] >> 4 & 0xf;
      /* If keeper_slot is 15, keeper will be the format key,
         which is a data key, which is treated as DK(0). */
      keeper = readkey(&seg->keys[keeper_slot]);
      if (!keeper) crash("JSEGMENT001 Can't readkey segment keeper");
      keeper = prx(keeper);
      if (!keeper) {
         coreunlock_node(seg);
         abandonj();
         return;
      }
      type = keeper->type & keytypemask;
      if (type == pagekey || type == nodekey || type == fetchkey
          || type == meterkey || type == segmentkey)
         keeper = &dk0;    /* These are treated as DK(0) */
      if (!(s15->nontypedata.dk7.databody[3] & 0x10)) {/* Pass node */
         cpup3node = seg;
         cpup3key.nontypedata.ik.item.pk.subject = (union Item *)seg;
         cpup3key.type = nodekey + prepared;
         cpup3key.databyte = segmentkeydatabyte;
         local_slot = s15->nontypedata.dk7.databody[4] >> 4 & 0xf;
         if (cpup3switch != CPUP3_UNLOCKED) crash("JSEG p3switch set");
         if (local_slot != 15 && local_slot != keeper_slot) {
            /* There is a local slot for the third key */
            if (type != startkey) keeper = &dk0;  /* restrict type
                of keeper due to difficulty of treating cpustore3key
                correctly in all cases. */
            if (puninv(&seg->keys[local_slot]) == puninv_cant) {
               crash("JSEGMENT002 Can't clean local slot");
            }
            cpup3switch = CPUP3_JUMPERKEY + local_slot;
            cpustore3key = *ld3(); /* get jumper's third key */
         }
         else cpup3switch = CPUP3_LOCKED;     /* Node locked above */
         cpuexitblock.keymask |= 2;
      }
      else coreunlock_node(seg);
      keyjump(keeper);
      return;
 
    case format_black:
      if (cpuordercode == 0) {  /* Create page R/O segment key */
         cpup1key = *key;
         cpup1key.databyte |= readonly;
         cpuordercode = 0;
         cpuarglength = 0;
         jsimple(8);  /* first key */
         return;
      }
      if (cpuordercode == KT) {
         if (segmentkeydatabyte & readonly)
            simplest(5 | (segmentkeydatabyte & 15)<<8 | 0x1000);
         else simplest(5 | (segmentkeydatabyte & 15)<<8);
         return;
      }
      else simplest(KT+2);
      return;
 
    case format_overlap:
      cputrapcode = 0x600 + 64;
      midfault();
      return;
   }
}
