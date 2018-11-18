/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include "sysdefs.h"
#include "kktypes.h"
#include "keyh.h"
#include "jpageh.h"
#include "cpujumph.h"
#include "gateh.h"
#include "geteh.h"
#include "locksh.h"
#include "cpumemh.h"
#include "prepkeyh.h"
#include "primcomh.h"
#include "memoryh.h"
#include "memomdh.h"
#include "queuesh.h"
#include "wsh.h"
#include "kernkeyh.h"
#include "ioworkh.h"
#include "alocpoth.h"
#include "locore.h"
#include "gdirecth.h"
#include "memutil.h"


void clear_page(          /* Clear a page */
   register CTE *p)       /* The page */
{
   memzero4n(map_window(QUICKWINDOW, p, MAP_WINDOW_RW),
             pagesize);
}
 
 
static void write_data(          /* Write data into the page */
   register CTE *p)                   /* The page */
{
   uchar *pd = map_window(3, p, 1)+cpuordercode-4096;

   if (cpumempg[0] || cpuexitblock.argtype != arg_memory) {
      if (args_overlap_with_page(p)) {
         Memcpy(cpuargpage, cpuargaddr, cpuarglength);
         Memcpy(pd, cpuargpage, cpuarglength);
      } else Memcpy(pd, cpuargaddr, cpuarglength);
   } else if ( 0 == movba2va(pd, cpuargaddr, cpuarglength) ) {
      if ( 0 == movba2va(cpuargpage, cpuargaddr, cpuarglength) ) {
         crash("JPAGE001 - Overlap with cpuargpage?");
      }
      Memcpy(pd, cpuargpage, cpuarglength);
   }
}
 
 
static void alter_page_state(   /* General page state change */
         /* N.B. The logical page may be in a different CTE on exit */
   register CTE *p,                        /* The page */
   void (*rtn)(CTE *))      /* The change state routine */
                     /* N.B. rtn must set up returned keys */
{
   if (p->flags & ctkernelreadonly &&    /* Can we make it writable */
          (p = gcleanmf(p)) == NULL) {
          /* If page could not be copied */
      enqueuedom(cpudibp->rootnode, &kernelreadonlyqueue);
      abandonj();
      return;
   }
   if (cpuexitblock.jumptype != jump_call) {/* Do it the slow way */
      corelock_page(p);                     /* Core lock the page */
      switch (ensurereturnee(0)) {
       case ensurereturnee_wait:
         coreunlock_page(1,p);
         abandonj();
         return;
       case ensurereturnee_overlap:
         coreunlock_page(2,p);
         midfault();
         return;
       case ensurereturnee_setup: break;
      }
      handlejumper();
      markpagedirty(p);
      (*rtn)(p);                       /* Change the page's state */
      coreunlock_page(3,p);
      cpuordercode = 0;
      cpuarglength = 0;
      if (! getreturnee()) return_message();
      return;
 
   } /* End do it the slow way */
   else {                                   /* Do it the fast way */
      markpagedirty(p);
      (*rtn)(p);                       /* Change the page's state */
      cpuordercode = 0;
      cpuarglength = 0;
      jsimplecall();
      return;
   } /* End do it the fast way */
}
 
 
void jpage(key)            /* Page key */
   struct Key *key;           /* The page key invoked */
{
   register CTE *p;
   PCFA mypcfa;
 
   /* Validate that the page key is not obsolete. */
   /* Also determine whether the page is in core. */
   if (!(key->type & prepared)) {   /* might be obsolete */
      tryprep(key);
      if (!(key->type & prepared)) {   /* still unprepared */
         if ((key->type & keytypemask) == datakey) {
            jdata(key);    /* we found it obsolete */
            return;
         }
         {
            struct codepcfa gv;
            gv = getalid(key->nontypedata.ik.item.upk.cda);
            switch (gv.code) {
             case get_ioerror: crash("jpagec io error");
             case get_wait:
               abandonj();   /* invoker has been queued */
               return;       /* cpudibp == NULL */
             case get_tryagain: break;
            }
            if (gv.pcfa->allocationid !=
                key->nontypedata.ik.item.upk.allocationid) {
               *key = dk0;   /* it is obsolete */
               jdata(key);
               return;
            }
            mypcfa.flags =
               gv.pcfa->flags; /* save gratis, checkread,
                     virtual zero bits */
         }
      }
   }
   /* Now either key is prepared (and page is in memory)
      or key is not prepared and page is not in memory
         and mypcfa.flags has its flags. */
   switch (cpuordercode) {
    case 0:              /* Create R/O page key */
      cpup1key = *key;
      cpup1key.databyte = readonly;
      cpuordercode = 0;
      cpuarglength = 0;
      jsimple(8);  /* first key */
      return;
 
    case 39:                      /* Clear page */
      if (key->databyte & readonly) {
         simplest(KT+2);
         return;
      }
#if !defined(diskless_kernel)
      if (!(key->type & prepared)) {
         if (mypcfa.flags & ADATAVIRTUALZERO) {
            simplest(0);  /* it is already zero */
            return;
         }
         /* See if we can clear it virtually */
         if (!(iosystemflags & CHECKPOINTMODE)) {
            /* ... direntthrottle */
            /* Clear page not in core. */
            Memcpy(mypcfa.cda, key->nontypedata.ik.item.upk.cda, 6);
            mypcfa.allocationid = key->nontypedata.ik.item.upk.allocationid;
            mypcfa.flags &= ADATAGRATIS; /* clear checkread bit */
            gdisetvz(&mypcfa); /* clear it virtually */
            simplest(0);
            return;
         } else {
            /* A checkpoint is in progress.
               We can't call gdisetvz because that would affect the
               next backup version of the page (not the current
               version).
               We must bring the page into memory. */
            if (prepkey(key) != prepkey_wait)
               crash("JPAGE778 thought it wasn't in core");
            abandonj();   /* invoker has been queued */
            return;
         }
      }
#endif /* !diskless_kernel */
      /* Clear page in memory. */
      p = (CTE *)key->nontypedata.ik.item.pk.subject;
      alter_page_state(p, clear_page);
      return;
 
    case 41:                      /* Test zeroness */
      if (!(key->type & prepared)) {
         simplest((mypcfa.flags & adatavirtualzero) == 0);
         return;
      }
      else { /* Page is in memory. */
         int i = 1024;
         uint32 *p = (uint32 *)map_window(1,
          ((CTE *)key->nontypedata.ik.item.pk.subject), 0 );
         for (; i == 0; i--) {  /* test one word at a time */
            if (*p != 0) { /* found a nonzero word */
               simplest(1);
               return;
            }
            p++;
         }
         simplest(0);
         return;
      }
    default:
      if (cpuordercode >= 4096 &&
             cpuordercode < 8192 &&
             !(key->databyte & readonly)) {
 
         /* Write page from string */
 
         if (!(key->type & prepared)) {
            /* Prepare the key to ensure the page is in memory. */
            switch (prepkey(key)) {
             case prepkey_notobj:
                crash("jpagec thought we validated the key!");
             case prepkey_wait:
                abandonj();   /* invoker has been queued */
                return;       /* cpudibp == NULL */
             case prepkey_prepared: break;
            }
         }
         p = (CTE *)key->nontypedata.ik.item.pk.subject;
         if (p->flags & ctcheckread) {
            if (cpuordercode < 4096+4 &&
                    cpuordercode + cpuarglength > 8192-4) {
                simplest(1);
                return;
            }
         }
         else if (cpuordercode + cpuarglength > 8192) {
            simplest(1);
            return;
         }
         alter_page_state(p, write_data);
         return;
      }
      else if (cpuordercode == KT) {
          if(key->databyte & readonly) simplest(0x1202);
          else simplest(0x202);
      }
      else simplest(KT+2);
   }
}
