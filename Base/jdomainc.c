/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "keyh.h"
#include "cpujumph.h"
#include "primcomh.h"
#include "locksh.h"
#include "queuesh.h"
#include "domainh.h"
#include "domamdh.h"
#include "unprndh.h"
#include "kschedh.h"
#include "gateh.h"
#include "domdefs.h"
#include "nodedefs.h"
#include "prepkeyh.h"
#include "memoryh.h"
#include "wsh.h"
#include "jdommdh.h"
#include "kernkeyh.h"

 
void jdomain(struct Key * key)         /* Handle jumps to domain keys */
/*
   cpudibp - has the jumper's DIB
   cpuordercode - has order code.
   The invoked key type is domainkey+prepared.
*/
{
   NODE *rn = (NODE *)key->nontypedata.ik.item.pk.subject;
   int rc;

   switch (cpuordercode) {
    case DOMAIN__GET_KEY+0:        /* Domain is prepared */
    case DOMAIN__GET_KEY+1:
    case DOMAIN__GET_KEY+2:
    case DOMAIN__GET_KEY+3:
    case DOMAIN__GET_KEY+4:
    case DOMAIN__GET_KEY+5:
    case DOMAIN__GET_KEY+6:
    case DOMAIN__GET_KEY+7:
    case DOMAIN__GET_KEY+8:
    case DOMAIN__GET_KEY+9:
    case DOMAIN__GET_KEY+10:
    case DOMAIN__GET_KEY+11:
    case DOMAIN__GET_KEY+12:
    case DOMAIN__GET_KEY+13:
    case DOMAIN__GET_KEY+14:
    case DOMAIN__GET_KEY+15:
      rc = prepare_domain(rn);    /* Ensure no involved in keys node */
      if (rc != prepdom_prepared) {
         if (rc == prepdom_malformed) {
            simplest(2);   /* Return code 2 for malformed dom */
            break;
         }
         if (rc == prepdom_wait) abandonj();
         else midfault();
         break;
      }
      cpuordercode -= DOMAIN__GET_KEY - NODE__FETCH;
      jnode1(rn->pf.dib->keysnode);
      break;
 
 
    case DOMAIN__SWAP_KEY+0:       /* Domain is prepared */
    case DOMAIN__SWAP_KEY+1:
    case DOMAIN__SWAP_KEY+2:
    case DOMAIN__SWAP_KEY+3:
    case DOMAIN__SWAP_KEY+4:
    case DOMAIN__SWAP_KEY+5:
    case DOMAIN__SWAP_KEY+6:
    case DOMAIN__SWAP_KEY+7:
    case DOMAIN__SWAP_KEY+8:
    case DOMAIN__SWAP_KEY+9:
    case DOMAIN__SWAP_KEY+10:
    case DOMAIN__SWAP_KEY+11:
    case DOMAIN__SWAP_KEY+12:
    case DOMAIN__SWAP_KEY+13:
    case DOMAIN__SWAP_KEY+14:
    case DOMAIN__SWAP_KEY+15:
      rc = prepare_domain(rn);    /* Ensure no involved in keys node */
      if (rc != prepdom_prepared) {
         if (rc == prepdom_malformed) {
            simplest(2);   /* Return code 2 for malformed dom */
            break;
         }
         if (rc == prepdom_wait) abandonj();
         else midfault();
         break;
      }
      cpuordercode -= DOMAIN__SWAP_KEY - NODE__SWAP;
      jnode1(rn->pf.dib->keysnode);
      break;
 
 
    case DOMAIN__MAKE_AVAILABLE:
      if (! dry_run_prepare_domain(rn, 0)) break;
      handlejumper();              /* End of dry run */
 
/*  if there is a stall queue on this domain, it must be restarted.  */
/*  Makeready() below would put the first stallee on the CPU queue   */
/*  but require the worrier for all the rest.  This operation on a   */
/*  domain suggests that worrying is not the best way to work off    */
/*  any stall queue.                                                 */

      while(rn->flags & NFREJECT) {
          rundom( (NODE *) ((char *)(rn->rightchain) - 
                     ((char *)(&rn->domhookkey)-(char *)rn) ));
      }

      zapresumes(rn->pf.dib);
      zapprocess(rn);
 
      if (rn->pf.dib->readiness & BUSY) {
         makeready(rn->pf.dib);     /* likely only clear BUSY */
                                    /* as stall queue drained above */
         cpuordercode = 1;
      }
      else cpuordercode = 0;
 
      coreunlock_node(rn);
      cpuexitblock.keymask = 0;
      cpuarglength = 0;
      if (! getreturnee()) return_message();
      break;
 
 
    case DOMAIN__MAKE_FAULT_EXIT:
      if (! dry_run_prepare_domain(rn, 0)) break;
      handlejumper();              /* End of dry run */
 
      if (rn->pf.dib->readiness & BUSY) {
         cpuordercode = 1;
         cpuexitblock.keymask = 0;           /* Return dk(0) */
      }
      else {
         rn->pf.dib->readiness |= BUSY;
 
         if (rn->domhookkey.type == pihk)
            zaphook(rn);     /* Remove from worry queue */
         cpuordercode = 0;
         cpup1key.type = resumekey+prepared;  /* Set up fault key */
         cpup1key.databyte = faultresume;
         cpup1key.nontypedata.ik.item.pk.subject = (union Item *)rn;
         cpuexitblock.keymask = 8;           /* Return first key */
      }
      coreunlock_node(rn);                   /* Untie node */
 
      cpuarglength = 0;                      /* No returned string */
      if (! getreturnee()) return_message();
      break;
 
 
    case DOMAIN__MAKE_BUSY:
      if (! dry_run_prepare_domain(rn, 0)) break;
      handlejumper();              /* End of dry run */
 
      zapresumes(rn->pf.dib);
      zapprocess(rn);
 
      if (rn->pf.dib->readiness & BUSY) {
         cpuordercode = 1;             /* Already busy */
      }
      else {                           /* Wasn't busy */
         rn->pf.dib->readiness |= BUSY;   /* Mark as busy */
         if (rn->domhookkey.type == pihk)
            zaphook(rn);     /* Remove from worry queue */
         cpuordercode = 0;             /* Made it busy */
      }
      coreunlock_node(rn);                   /* Untie node */
 
      cpup1key.type = resumekey+prepared;  /* Set up fault key */
      cpup1key.databyte = faultresume;
      cpup1key.nontypedata.ik.item.pk.subject = (union Item *)rn;
      cpuexitblock.keymask = 8;           /* Return first key */
      cpuarglength = 0;                      /* No returned string */
      if (! getreturnee()) return_message();
      break;
 
 
    case DOMAIN__MAKE_RETURN_EXIT:
      if (! dry_run_prepare_domain(rn, 0)) break;
      handlejumper();              /* End of dry run */
 
      if (rn->pf.dib->readiness & BUSY) {
         cpuordercode = 1;
         cpuexitblock.keymask = 0;           /* Return dk(0) */
      }
      else {
         rn->pf.dib->readiness |= BUSY;
         if (rn->domhookkey.type == pihk)
            zaphook(rn);     /* Remove from worry queue */
         cpuordercode = 0;
         cpup1key.type = resumekey+prepared;  /* Set up fault key */
         cpup1key.databyte = returnresume;
         cpup1key.nontypedata.ik.item.pk.subject = (union Item *)rn;
         cpuexitblock.keymask = 8;           /* Return first key */
      }
      coreunlock_node(rn);                   /* Untie node */
      cpuarglength = 0;                      /* No returned string */
      if (! getreturnee()) return_message();
      break;
 
 
    case DOMAIN__GET+1:            /* Preparation mode unknown */
    case DOMAIN__GET+2:
    case DOMAIN__GET+3:
    case DOMAIN__GET+10:
    case DOMAIN__GET+11:
      cpuordercode -= DOMAIN__GET - NODE__FETCH;
      jnode1(rn);
      break;
 
 
    case DOMAIN__SWAP+1:
    case DOMAIN__SWAP+2:
    case DOMAIN__SWAP+10:
    case DOMAIN__SWAP+11:
      cpuordercode -= DOMAIN__SWAP - NODE__SWAP;
      jnode1(rn);
      break;
 
 
    case DOMAIN__MAKE_START:
      cpup1key.type = startkey+prepared;  /* Set up start key */
      pad_move_arg(&cpup1key.databyte, 1);
      cpup1key.nontypedata.ik.item.pk.subject = (union Item *)rn;
      cpuordercode = 0;                   /* And return code */
      cpuarglength = 0;                      /* No returned string */
      corelock_node(5, rn);                  /* Tie node down */
      jsimple(8);                          /* While we finish jump, First key */
      coreunlock_node(rn);                /* Untie node */
      break;                             /* And return */
 
    case DOMAIN__COMPARE:
      {  
         struct Key *s = ld1();
/*
         N.B. tryprep, in ld1, will have prepared the caller's key if
              it is to "rn" since "rn" is already in node space
*/
         if (s->type & prepared &&
             (NODE *)s->nontypedata.ik.item.pk.subject == rn &&
               ({uchar type = s->type & keytypemask;
                type == startkey || ((type == resumekey ) &&
                    s->databyte != 4);}) ) {
               cpuordercode = 0;
         }
         else cpuordercode = 1;
         cpuarglength = 0;
         jsimple(0);   /* no keys */
         break;
      }  
 
    default: if (cpuordercode == KT) simplest(7);
      else md_jdomain(rn);  /* machine-dependent order codes */
      break;
   } /* End switch on order code */
}

