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
#include "kernkeyh.h"
 
 
static void make_key_sensory(void) /* Make cpup1key a sensory version */
{
   switch (cpup1key.type & keytypemask) {
    case datakey:
      return;
    case segmentkey:
      cpup1key.databyte |= readonly | nocall;
      return;
    case pagekey:
      cpup1key.databyte |= readonly;
      return;
    case nodekey:
    case fetchkey:
    case sensekey:
      cpup1key.type = (cpup1key.type & ~keytypemask) | sensekey;
      cpup1key.databyte |= readonly | nocall;
      return;
    case misckey:
      switch (cpup1key.nontypedata.dk11.databody11[0]) {
       case returnermisckey:
       case datamisckey:
       case discrimmisckey:
         return;
       default: ;             /* Fall through to default action */
      }
    default: cpup1key = dk0;         /* All others become DK0 */
   }
}
 
 
void jsense(        /* Handle jumps to sense keys */
   struct Key *key)
{
   register NODE *node = (NODE *)key->nontypedata.ik.item.pk.subject;
 
   if (cpuordercode < 16 && cpuordercode >= 0) {
      register struct Key *s = node->keys + cpuordercode-NODE__FETCH;
 
      if (cpuexitblock.jumptype == jump_call &&
         (!(s->type & involvedr) ||
            (node != cpudibp->rootnode
             && (s = readkey(s)) != NULL))) { /* Fetch the fast way */
         cpuordercode = 0;
         cpuarglength = 0;
         cpuexitblock.keymask = 8;
         cpup1key = *s;
         cpup1key.type &= ~involvedw;
         make_key_sensory();
         jsimplecall();
         zapresumes(cpudibp);
         return;
      } /* End fetch the fast way */
      else {                             /* Fetch the slow way */
         corelock_node(15, node);   /* Core lock the node */
         switch (ensurereturnee(0)) {
          case ensurereturnee_wait:  {
             abandonj();
             return;
          }
          case ensurereturnee_overlap: {
             midfault();
             return;
          }
          case ensurereturnee_setup: handlejumper();
         }
      /* End dry run */
         cpup1key = *readkey(node->keys + cpuordercode-NODE__FETCH);
         cpup1key.type &= ~involvedw;
         coreunlock_node(node);
         make_key_sensory();
         cpuordercode = 0;
         cpuarglength = 0;
         cpuexitblock.keymask = 8;
         if (! getreturnee()) return_message();
         return;
      } /* End fetch the slow way */
   }
 
   if (cpuordercode == NODE__DATA_BYTE) {
      simplest(key->databyte);
      return;
   }
   if (cpuordercode == KT) simplest(1);
   else simplest(KT+2);
   return;
} /* End jsense */
