/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#include <string.h>
#include <limits.h>
#include "sysdefs.h"
#include "lli.h"
#include "keyh.h"
#include "cpujumph.h"
#include "gateh.h"
#include "locksh.h"
#include "wsh.h"
#include "prepkeyh.h"
#include "primcomh.h"
#include "queuesh.h"
#include "memoryh.h"
#include "memomdh.h"
#include "geteh.h"
#include "spaceh.h" 
#include "jnrangeh.h"
#include "jpageh.h"
#include "kernkeyh.h"
#include "devioh.h"
#include "ioworkh.h"
#include "gdirecth.h"
#include "alocpoth.h"
#include "diskless.h"
#include "getih.h"
#include "memutil.h"


static CTE *jprcte;
static PCFA *jprpcfa;
static int prangevalidatekey(   /* Initial processing for oc 1-4, 6 */
   struct Key *key,             /* the input range key */
   struct Key *pk1)                 /* The key to validate */
/* Validates passed key 0. */
/* Values returned are: */
#define pvk_invalid   0 /* key is not valid, or must wait for i/o.
     In this case, abandonj or simplest will have been called. */
#define pvk_incore    1 /* the page is in core. jprcte is set. */
#define pvk_notincore 2 /* the page is not in core. jprpcfa is set. */
/* In the above two cases, offset has the offset in the range. */
{
 
   if (pk1->databyte != 0 ||
       (pk1->type & keytypemask) != pagekey ) { /* not a r/w page key */
      simplest(-1);
      return pvk_invalid;
   }
   if (pk1->type & prepared) { /* prepared */
      jprcte = (CTE *)pk1->nontypedata.ik.item.pk.subject;
      if (checkcdainrange(jprcte->use.page.cda, key) == 0) {
         return pvk_invalid;
      }
      return pvk_incore;
   }
   else { /* not prepared */
      struct codepcfa cp;
      cp = validatepagekey(pk1);
      switch (cp.code) {
       case vpk_wait:
         return pvk_invalid;
       case vpk_ioerror:
         crash("jprangec io error");
       case vpk_obsolete:
         simplest(-1);
         return pvk_invalid;
       case vpk_current:
         if (checkcdainrange(cp.pcfa->cda, key) == 0) {
            return pvk_invalid;
         }
         jprpcfa = cp.pcfa;
         return pvk_notincore;
       default: crash("vnipeo"); // don't know whether this path is valid.
      }
   }
}
 
static void sever_page(         /* Sever a page */
   register CTE *p)                  /* The page to sever */
{
   if (p->devicelockcount) devicehd(p);  /* Halt active I/O */
   detpage(p);   /* Uninvolve any keys to the page */
   for (;;) {    /* Zap prepared keys to page (all uninvolved) */
      struct Key *k = (struct Key *)p->use.page.rightchain;
      if ((CTE *)k == p)
   break;
      p->use.page.rightchain = k->nontypedata.ik.item.pk.rightchain;
      *k = dk0;
   }
   p->use.page.leftchain = (union Item *)p;  /* End zap prepared keys */
   p->flags &= ~ctcheckread;        /* Remove check read status */
   if (p->flags & ctallocationidused) {
      p->use.page.allocationid += 1;
      if (!p->use.page.allocationid) crash("Page allocationid wrapped");
      p->flags &= ~ctallocationidused;
   }
   cpup1key.type = pagekey+prepared;
   cpup1key.databyte = 0;
   cpup1key.nontypedata.ik.item.pk.subject = (union Item *)p;
   cpuexitblock.keymask = 8;
}
 
 
static void sever_and_clear_page(  /* Sever and clear a page */
   register CTE *p)                     /* The page */
{
   sever_page(p); clear_page(p);
}
 
 
static void alter_page_state( /* General page state change */
         /* N.B. The logical page may be in a different CTE on exit */
   CTE *p,                  /* the page to alter */
   void (*rtn)(CTE *))      /* the change state routine */
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
         coreunlock_page(4,p);
         abandonj();
         return;
       case ensurereturnee_overlap:
         coreunlock_page(5,p);
         midfault();
         return;
       case ensurereturnee_setup: break;
      }
      handlejumper();
      markpagedirty(p);
      (*rtn)(p);                       /* Change the page's state */
      coreunlock_page(6,p);
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
 
 
static void severandclearnotincore(void)
{
   if (iosystemflags & CHECKPOINTMODE) {
      /* A checkpoint is in progress.
         Don't mess with the directory. */
      struct Key *key = prep_passed_key1(); /* bring it in */
      if (!key) {
         abandonj();
         return;
      }
      /* Got it. It must have been virtual zero. */
      if (!(jprpcfa->flags & adatavirtualzero))
         crash("JPRANGE647 not virtual zero?");
      /* Sever. No need to clear since it is already zero. */
      alter_page_state(&key->nontypedata.ik.item.pk.subject->cte,
                       sever_page);
   } else {
      PCFA pcfa = *jprpcfa; /* Copy so we can modify */
      /*... direntthrottle */
      if (pcfa.allocationid++ == ULONG_MAX)
         crash("JPRANGE648 allocationid overflow");
      pcfa.flags &= ~ADATACHECKREAD;
      gdisetvz(&pcfa);
      simplest(0);
   }
}
 

void jprange(          /* Page range key */
   struct Key *key)           /* The range key invoked */
{
   register CTE *p;
   unsigned char *cda;
   unsigned char flags;
   struct codepcfa gv;
   struct Key *pk1,ik1;

#if diskless_kernel
   if(cpuordercode == 42) {
      pk1 = ld1();
      switch (prangevalidatekey(key,pk1)) {
        case pvk_invalid: 
            return;
        case pvk_notincore:
            simplest(-1);  /* this isn't interesting */
            return;
        case pvk_incore:
            alter_page_state(jprcte, sever_page);
            cpuexitblock.keymask=0;
            cpup1key=dk0;
            gspdetpg(jprcte);

            jprcte->use.page.leftchain = (union Item *)jprcte;
            jprcte->use.page.rightchain = jprcte->use.page.leftchain;
            jprcte->flags=jprcte->iocount=jprcte->extensionflags=jprcte->devicelockcount=0;

// prom_printf("JPRANGE(42) %x %x %x %x %x %x\n",*jprcte->use.page.cda,
//       *(jprcte->use.page.cda+1),*(jprcte->use.page.cda+2),*(jprcte->use.page.cda+3),
//       *(jprcte->use.page.cda+4),*(jprcte->use.page.cda+5));
 
//         unchain_hash(&apagechainheads[cdahash(jprcte->use.page.cda) & pagechainhashmask],
//            jprcte);

            Memset(&(jprcte->use.page.cda),0,6);
            jprcte->ctefmt=FreeFrame;
            gspmpfa(jprcte);  /* free page frame */   
            return;
      }
   }
#endif
 
   switch (cpuordercode) {


    case 0:              /* Create page key */
    case 5:              /* Create page key - no wait */
    case 9:              /* Create page key and clear - no wait */
      if (9 == cpuordercode) cda = validaterelativecda6(key);
      else cda = validaterelativecda(key);

      if (cda == NULL) return;
      p = srchpage(cda);
 
      if (p == NULL) {
#if diskless_kernel
         static PCFA newpcfa = {{0}};
         {LLI i; b2lli(cda, 6, &i);
          if(i.hi == 0 && i.low<4096)
            crash("Probably an undefined primordial page key.");}
         p = gspgpage() /* Get a frame. */;
         if(p==NULL) crash("Really out of pages!");
         Memcpy(&newpcfa.cda, cda, 6);
         setupvirtualzeropage(&newpcfa, p);
         cpup1key.type = pagekey+prepared;
         cpup1key.nontypedata.ik.item.pk.subject = (union Item *)p;
#else
         gv = getalid(cda);
         switch (gv.code) {
          case get_ioerror:
            simplest(3);
            return;
          case get_wait:
            if (cpuordercode != 0 &&
                cpudibp->rootnode->domhookkey.nontypedata.ik.item.
                    pk.subject == (union Item *)&rangeunavailablequeue){
               /* abort wait and return 2 */
               if (!(cpudibp->rootnode->preplock & 0x80))
                  crash("JPRANGE001 Actor not preplocked after get");
               zaphook(cpuactor);
               simplest(2);
               return;
            }
            abandonj();
            return;
          case get_gotpcfa:
            cpup1key.type = pagekey;
            Memcpy(cpup1key.nontypedata.ik.item.upk.cda,
                   gv.pcfa->cda, 6);
            cpup1key.nontypedata.ik.item.upk.allocationid =
               gv.pcfa->allocationid;
            break;
          default: crash("JPRANGE765 bad code from getalid");
         }
#endif
      }
      else {
         cpup1key.type = pagekey+prepared;
         cpup1key.nontypedata.ik.item.pk.subject = (union Item *)p;
      }

#if !diskless_kernel
      if (9 == cpuordercode){
	 if (p || (p = srchpage(gv.pcfa->cda))){
	    clear_page(p);
	 } else {
	    gdisetvz(gv.pcfa);
	}
      }
#endif
	    

      cpup1key.databyte = 0;
      cpuordercode = 0;
      cpuarglength = 0;
      jsimple(8);  /* first key */
      return;
 
    case 1:                       /* Get CDA */
      pk1 = ld1();
      switch (prangevalidatekey(key,pk1)) {
       case pvk_invalid: return;
       case pvk_notincore: flags = jprpcfa->flags; break;
       case pvk_incore: flags = jprcte->flags; break;
      }
      if (flags & ctgratis) cpuordercode = 1;
      else cpuordercode = 0;
      cpuexitblock.argtype = arg_regs;
      cpuargaddr = (char *)&offset.low;
      cpuarglength = 4;
      jsimple(0);  /* no key */
      return;

    case 2:                       /* Sever page */
      pk1 = ld1();
      switch (prangevalidatekey(key,pk1)) {
       case pvk_invalid: return;
       case pvk_incore:
         alter_page_state(jprcte, sever_page);
         return;
       case pvk_notincore:
         if (jprpcfa->flags & adatavirtualzero) {
            severandclearnotincore();
            return;
         }
         /* we can't create a directory entry (with an increased
            allocationid) for this page because it may not be in
            the current swap area. Too bad, must bring it in. */
         if (prep_passed_key1() != NULL)
            crash("thought it wasn't incore");
         abandonj();
         return;
      }
 
    case 3:                       /* Make gratis */
    case 4:                       /* Make non-gratis */
      pk1 = ld1();
      switch (prangevalidatekey(key,pk1)) {
       case pvk_invalid: return;
       case pvk_notincore: flags = jprpcfa->flags; break;
       case pvk_incore: flags = jprcte->flags; break;
      }
      /* ***** More work needed here to handle in core vs. not!! */
      if (cpuordercode == 3) {/* make gratis */;}
      else {/* make non-gratis */;}
      simplest(0);  /* for now just say it's ok */
      return;
 
    case 6:                       /* Sever and clear page */
      pk1 = ld1();
      switch (prangevalidatekey(key,pk1)) {
       case pvk_invalid: return;
       case pvk_notincore: severandclearnotincore(); return;
       case pvk_incore:
         alter_page_state(jprcte, sever_and_clear_page);
         return;
      }

    case 8:             /* Sever and clear page by cda */
      cda = validaterelativecda6(key);

      if (cda == NULL) return;
      p = srchpage(cda);
 
      if (p == NULL) {
#if diskless_kernel
         simplest(0);
         return;
#else
         gv = getalid(cda);
         switch (gv.code) {
          case get_ioerror:
            simplest(3);
            return;
          case get_wait:
            abandonj();
            return;
          case get_gotpcfa:
            ik1.type = pagekey;
            Memcpy(ik1.nontypedata.ik.item.upk.cda,
                   gv.pcfa->cda, 6);
            ik1.nontypedata.ik.item.upk.allocationid =
               gv.pcfa->allocationid;
            break;
          default: crash("JPRANGE765a bad code from getalid");
         }
#endif
      }
      else {
         ik1.type = pagekey+prepared;
         ik1.nontypedata.ik.item.pk.subject = (union Item *)p;
      }
      switch (prangevalidatekey(key,&ik1)) {
       case pvk_invalid: return;
       case pvk_notincore: severandclearnotincore(); return;
       case pvk_incore:
         alter_page_state(jprcte, sever_and_clear_page);
      } 
      return;
 
    default:
      if (cpuordercode == KT) simplest(770);
      else simplest(KT+2);
   }
}
