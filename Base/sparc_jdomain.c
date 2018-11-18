/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/* jdom88kc.c - machine-dependent domain key call for 88000 */
/* The header for this file is jdommdh.h */

#include <string.h>
#include "sysdefs.h"
#include "keyh.h"
#include "cpujumph.h"
#include "sparc_domdefs.h"
#include "nodedefs.h"
#include "domdefs.h"
#include "domainh.h"
#include "domamdh.h"
#include "primcomh.h"
#include "locksh.h"
#include "gateh.h"
#include "memoryh.h"
#include "jdommdh.h"
#include "prepkeyh.h"
#include "kernkeyh.h"
#include "psr.h"
#include "wsh.h"
#include "locore.h"
#include "memutil.h"

static void clean_windows(struct DIB * dm){}

/***********************+++++++++++++++++++****************************/
/*  WARNING:  We have no way to share headers with domainland yet     */
/*            The following structure is repeated in domain.h         */
/***********************+++++++++++++++++++****************************/

 struct Domain_SPARCCopyInstructions {
    unsigned copyregisters:1;
    unsigned copyfloats:1;
    unsigned copykeys:1;
    unsigned copyslot10:1;
    unsigned copyslot11:1;
    unsigned substitutedomain:1;

    unsigned char domainkeyslot;
    unsigned short keymask;
    unsigned long unused;
 };


void md_jdomain(NODE * rn)
{
   int rc;

   struct Domain_SPARCCopyInstructions dci;

   switch (cpuordercode) {
   
    case DOMAIN__SWAP+3:
      {
         struct Key tempkey;
 
         tempkey = *ld1();                  /* Save jumper's key */
         if (! dry_run_prepare_domain(rn, 0)) return;
         handlejumper();              /* End of dry run */
 
         if (puninv(&rn->dommemroot))
            crash("Sparc_jdomain001 What's preplocked??");
         cpup1key = rn->dommemroot;     /* Set up returned key */
         clean(&rn->dommemroot);
         rn->dommemroot = tempkey;      /* Copy in new key */
         /* rn is prepasdomain, therefore dirty */
         if (rn->dommemroot.type & prepared)
                halfprep(&rn->dommemroot);
         coreunlock_node(rn);                   /* Untie node */
         cpuordercode = 0;
         cpuexitblock.keymask = 8;          /* Return first key */
         cpuarglength = 0;                  /* No returned string */
         if (! getreturnee()) return_message();
         return;
      }
 
    case DOMAIN__REPLACE_MEMORY:
      {
         struct Key tempkey;
         unsigned long ip;
 
         tempkey = *ld1();                  /* Save jumper's key */
         if (! dry_run_prepare_domain(rn, 0)) return;
 
      /* Delay end of dry run until jumper's string has been accessed */
      /* This is permissible since we aren't chainging the readiness */
      /* state of the target domain (which may be the jumper) */
 
         /* Set program counter */
         pad_move_arg((char *)&ip, 4);
         set_inst_pointer(rn->pf.dib, ip);
 
         handlejumper();              /* End of dry run */
 
         if (puninv(&rn->dommemroot))
            crash("Sparc_jdomain003 What's preplocked??");
         cpup1key = rn->dommemroot;     /* Set up returned key */
         clean(&rn->dommemroot);
         /* rn is prepasdomain, therefore dirty */
         rn->dommemroot = tempkey;      /* Copy in new key */
         if (rn->dommemroot.type & prepared)
                halfprep(&rn->dommemroot);
         coreunlock_node(rn);                   /* Untie node */
         cpuordercode = 0;
         cpuexitblock.keymask = 8;          /* Return first key */
         cpuarglength = 0;                  /* No returned string */
         if (! getreturnee()) return_message();
         return;
      }


    case Domain_GetSparcControl:
      rc = prepare_domain(rn);
      if (rc != prepdom_prepared) {
         if (rc == prepdom_malformed) {
            simplest(2);   /* Return code 2 for malformed dom */
            return;
         }
         if (rc == prepdom_wait) abandonj();
         else midfault();
         return;
      }
      cpuexitblock.argtype = arg_regs;    /* String in kernel memory */
      cpuargaddr = cpuargpage;
      cpuarglength = format_control(rn->pf.dib, (unsigned char *)cpuargaddr);
      cpuordercode = 0;                   /* And return code */
      jsimple(0);  /* no keys */
      return;
 
 
    case Domain_PutSparcControl:
      if (! dry_run_prepare_domain(rn, 0)) return;
      handlejumper();              /* End of dry run */
      {
         struct DIB *dib = rn->pf.dib;
         /* PC, nPC, and PSR */
         pad_move_arg((char*)&dib->pc, 12);
                                        
         /* swallow zeroes and move trapcode */
         pad_move_arg((char*)&dib->Trapcode, 2);
         pad_move_arg((char*)&dib->Trapcode, 2);
         if(dib->Trapcode) dib->readiness |= TRAPPED;
             else dib->readiness &= ~TRAPPED;
         /* trap code extension */
         pad_move_arg((char*)&dib->trapcodeextension, 8);
         /* Floating point */
         pad_move_arg((char*)&dib->fsr, 12);
         if (dib->psr & PSR_S) dib->permits |= GATEJUMPSPERMITTED;
         else dib->permits &= ~GATEJUMPSPERMITTED;
         
         dib->psr = ((dib->psr & PSR_ICC) | PSR_S);
      }
      cpuordercode = 0;
      coreunlock_node(rn);
 
      cpuexitblock.keymask = 0;
      cpuarglength = 0;                      /* No returned string */
      if (! getreturnee()) return_message();
      return;
 
 
    case Domain_ResetSparcControl:
      if (cpuarglength & 3) {/* Length a multiple of 4? */
         simplest(3);  /* no, then rc=3 */
         return;
      }
      if (! dry_run_prepare_domain(rn, 0)) return;
      handlejumper();              /* End of dry run */
      {
         struct DIB *dib = rn->pf.dib;
         int len = (cpuarglength < 12 ? cpuarglength : 12);
         /* PC, nPC, PSR */
         pad_move_arg((char*)&dib->pc, len);
                                        
         /* swallow zeroes and move trapcode */
         len = (cpuarglength < 4 ? cpuarglength : 4);
         pad_move_arg((char*)&dib->Trapcode, len);
         pad_move_arg((char*)&dib->Trapcode, len);
         /* trap code extension */
         len = (cpuarglength < 8 ? cpuarglength : 8);
         pad_move_arg((char*)&dib->trapcodeextension, len);
         /* Floating Point */
         len = (cpuarglength < 12 ? cpuarglength : 12);
         pad_move_arg((char*)&dib->fsr, len);

         if (dib->psr & PSR_S) dib->permits |= GATEJUMPSPERMITTED;
         else dib->permits &= ~GATEJUMPSPERMITTED;
         
         dib->psr = (dib->psr & PSR_ICC) | PSR_S;
        
         clear_trapcode(dib);   /* Zero the trapcode */
      }
      cpuordercode = 0;
      coreunlock_node(rn);
 
      cpuexitblock.keymask = 0;
      cpuarglength = 0;                      /* No returned string */
      if (! getreturnee()) return_message();
      return;
 
 
    case Domain_GetSparcRegs:
      if (! dry_run_prepare_domain(rn, 1)) return;
      handlejumper();              /* End of dry run */
 
      if (getreturnee()) {    /* No returnee */
         coreunlock_node(rn);
         return;
      }
      if (rn->prepcode != prepasdomain) { /* Not prepared as domain */
         unpreplock_node(cpujenode);  /* Reset returnee */
         unsetupdestpage();           /* and have process disappear */
         coreunlock_node(rn);
         return;                      /* (Jumper has been handled) */
      }
      coreunlock_node(rn);
 
      cpuordercode = 0;
      cpuexitblock.keymask = 0;
      cpuexitblock.argtype = arg_regs;
      clean_windows(rn->pf.dib);
      cpuargaddr = cpuargpage;
      Memcpy(cpuargpage, rn->pf.dib->regs, sizeof rn->pf.dib->regs);
      Memcpy(cpuargpage+sizeof rn->pf.dib->regs, 
             &rn->pf.dib->backset[rn->pf.dib->backalloc], sizeof (backwindow)); 
      cpuarglength = sizeof rn->pf.dib->regs + sizeof (backwindow);
      return_message();
      return;
 
 
    case Domain_PutSparcRegs:
      if (! dry_run_prepare_domain(rn, 0)) return;
 
      /* Delay end of dry run until jumper's string has been accessed */
      /* This is permissable since we aren't changing the readiness */
      /* state of the target domain (which may be the jumper) */
      {
         struct DIB *dib = rn->pf.dib;
 
         pad_move_arg((char*)dib->regs,  /* N.B. No distructive overlap */
                sizeof dib->regs);       /* w/regs argument since move  */
                                         /* is to first byte of regs */
         pad_move_arg((char*)&dib->backset[dib->backalloc],
                sizeof (backwindow));
      }
      handlejumper();              /* End of dry run */
 
      coreunlock_node(rn);
 
      cpuordercode = 0;
      cpuexitblock.keymask = 0;
      cpuarglength = 0;                      /* No returned string */
      if (! getreturnee()) return_message();
      return;
 
 
    case Domain_GetSparcStuff:
      rc = prepare_domain(rn);
      if (rc != prepdom_prepared) {
         if (rc == prepdom_malformed) {
            simplest(2);   /* Return code 2 for malformed dom */
            return;
         }
         if (rc == prepdom_wait) abandonj();
         else midfault();
         return;
      }
      cpuexitblock.argtype = arg_regs;    /* String in kernel memory */
      cpuargaddr = cpuargpage;
      Memcpy(cpuargaddr, 
             (char *)rn->pf.dib->regs, sizeof rn->pf.dib->regs);
      Memcpy(cpuargaddr + sizeof rn->pf.dib->regs,
             (char *)&rn->pf.dib->backset[rn->pf.dib->backalloc],
             sizeof (backwindow));
      cpuarglength = format_control(rn->pf.dib,
                       (unsigned char *)cpuargaddr
                                   +sizeof rn->pf.dib->regs+sizeof (backwindow))
                     + sizeof rn->pf.dib->regs + sizeof (backwindow);
      cpuordercode = 0;                   /* And return code */
      jsimple(0);  /* no keys */
      return;
 
 
   case Domain_PutSparcStuff:
      if (! dry_run_prepare_domain(rn, 0)) return;
      handlejumper();              /* End of dry run */
 
      {
         struct DIB *dib = rn->pf.dib;
 
         pad_move_arg((char*)&dib->regs, /* N.B. No distructive overlap */
                sizeof dib->regs);       /* w/regs argument since move  */
                                         /* is to first byte of regs */
         pad_move_arg((char*)&dib->backset[dib->backalloc],
                sizeof (backwindow));
 
        /* PC, nPC, and PSR */
         pad_move_arg((char*)&dib->pc, 12);
                                        
        /* swallow zeroes and move trapcode */
         pad_move_arg((char*)&dib->Trapcode, 2);
         pad_move_arg((char*)&dib->Trapcode, 2);
        /* trap code extension */
         pad_move_arg((char*)&dib->trapcodeextension, 8);
        /* floating point */
         pad_move_arg((char*)&dib->fsr, 12);
         
         if (dib->psr & PSR_S) dib->permits |= GATEJUMPSPERMITTED;
         else dib->permits &= ~GATEJUMPSPERMITTED;
         
         dib->psr = (dib->psr & PSR_ICC) | PSR_S;
      }
      coreunlock_node(rn);
 
      cpuordercode = 0;
      cpuexitblock.keymask = 0;
      cpuarglength = 0;                      /* No returned string */
      if (! getreturnee()) return_message();
      return;
 
 
    case Domain_ResetSparcStuff:
      if (cpuarglength & 3) {/* Length a multiple of 4? */
         simplest(3);  /* no, then rc=3 */
         return;
      }
      if (! dry_run_prepare_domain(rn, 0)) return;
      handlejumper();              /* End of dry run */
      {
         struct DIB *dib = rn->pf.dib;
         int len;
 
         pad_move_arg((char*)&dib->regs, /* N.B. No destructive overlap */
                sizeof dib->regs);       /* w/regs argument since move  */
                                         /* is to first byte of regs */
         pad_move_arg((char*)&dib->backset[dib->backalloc],
                sizeof (backwindow));

         len = (cpuarglength < 12 ? cpuarglength : 12);
         /* PC, nPC, and PSR */
         pad_move_arg((char*)&dib->pc, len);
                                        
         /* swallow zeroes and trapcode */
         len = (cpuarglength < 4 ? cpuarglength : 4);
         pad_move_arg((char*)&dib->trapcodeextension, len);
         /* trap code extension */
         len = (cpuarglength < 8 ? cpuarglength : 8);
         pad_move_arg((char*)&dib->trapcodeextension, len);
         /* Floating point */
         len = (cpuarglength < 12 ? cpuarglength : 12);
         pad_move_arg((char*)&dib->fsr, len);
         
         if (dib->psr & PSR_S) dib->permits |= GATEJUMPSPERMITTED;
         else dib->permits &= ~GATEJUMPSPERMITTED;
         
         dib->psr = (dib->psr & PSR_ICC) | PSR_S;
         
         dib->Trapcode = 0;  /* Zero the trapcode */
         dib->readiness &= ~TRAPPED;
      }
      coreunlock_node(rn);
 
      cpuordercode = 0;
      cpuexitblock.keymask = 0;
      cpuarglength = 0;                      /* No returned string */
      if (! getreturnee()) {
         return_message();
      }
      return;
 
 
    case Domain_GetSparcFQ:
      rc = prepare_domain(rn);
      if (rc != prepdom_prepared) {
         if (rc == prepdom_malformed) {
            simplest(2);   /* Return code 2 for malformed dom */
            return;
         }
         if (rc == prepdom_wait) abandonj();
         else midfault();
         return;
      }
      cpuexitblock.argtype = arg_regs;    /* String in kernel memory */
      cpuargaddr = (char*)rn->pf.dib->deferred_fp;
      cpuarglength = 0;

      {  int i;
         struct DIB *dib = rn->pf.dib;

         if (dib->psr & PSR_EF) clean_fp(dib);

         for (i=0; i<4; i++) {
            if (dib->deferred_fp[i].address 
                || dib->deferred_fp[i].instruction) {
               cpuarglength += sizeof (pipe_entry);
            } else break;
         }
      }
      cpuordercode = 0;                   /* And return code */
      jsimple(0);  /* no keys */
      return;
      
      
    case Domain_GetSparcOldWindows:
      rc = prepare_domain(rn);
      if (rc != prepdom_prepared) {
         if (rc == prepdom_malformed) {
            simplest(2);   /* Return code 2 for malformed dom */
            return;
         }
         if (rc == prepdom_wait) abandonj();
         else midfault();
         return;
      }
      {  struct DIB *dib = rn->pf.dib;

         clean_windows(dib);
         cpuexitblock.argtype = arg_regs;    /* String in kernel memory */
         if (dib->backalloc < dib->backdiboldest) {  /* Windows wrap */
            int len1 = (32 - dib->backdiboldest) * sizeof (backwindow);
            int len2 = dib->backalloc * sizeof (backwindow);

            Memcpy(cpuargpage, &dib->backset[dib->backdiboldest], len1);
            Memcpy(cpuargpage+len1, dib->backset, len2);
            cpuargaddr = cpuargpage;
         } else cpuargaddr = (char*)&dib->backset[dib->backdiboldest];
         cpuarglength = (dib->backalloc - dib->backdiboldest)*sizeof (backwindow);
      }
      cpuordercode = 0;                   /* And return code */
      jsimple(0);  /* no keys */
      return;
      
      
    case Domain_ClearSparcFQ:
      if (! dry_run_prepare_domain(rn, 0)) return;
      handlejumper();              /* End of dry run */
      {  struct DIB *dib = rn->pf.dib;

         if (dib->psr & PSR_EF) clean_fp(dib);

         Memset(dib->deferred_fp, 0, sizeof dib->deferred_fp);
      }
      coreunlock_node(rn);
      cpuordercode = 0;                   /* And return code */
      cpuexitblock.keymask = 0;           /* Return no keys */
      cpuarglength = 0;                      /* No returned string */
      if (! getreturnee()) {
         return_message();
      }
      return;
 
 
    case Domain_ClearSparcOldWindows:
      if (! dry_run_prepare_domain(rn, 0)) return;
      handlejumper();              /* End of dry run */
      {  struct DIB *dib = rn->pf.dib;

         clean_windows(dib);
         dib->backdiboldest = dib->backalloc;
      }
      coreunlock_node(rn);
      cpuordercode = 0;                   /* And return code */
      cpuexitblock.keymask = 0;           /* Return no keys */
      cpuarglength = 0;                      /* No returned string */
      if (! getreturnee()) {
         return_message();
      }
      return;
 
    case Domain_SparcCopyCaller:
     {
         int i;
         unsigned short mask;
         struct Key tempkey,*s;

      if (rn == cpudibp->rootnode) {
         simplest(KT+2);   /* Silly request, denied */
         return;
      }

      rc = prepare_domain(rn);

/* clearly my dib can't go away.. but in an MP the target DIB can */

      if (rc != prepdom_prepared) {
         if (rc == prepdom_malformed) {
            simplest(2);   /* Return code 2 for malformed dom */
            return;
         }
         if (rc == prepdom_wait) abandonj();
         else midfault();
         return;
      }

      pad_move_arg((char* )&dci,sizeof(dci));

/* the source of all moves is cpudibp */
/* the destination of all moves is rn->pf.dib */ 

      if(dci.copyregisters) {
          for(i=0;i<16;i++) {
              rn->pf.dib->regs[i]=cpudibp->regs[i];
          }
          rn->pf.dib->backset[rn->pf.dib->backalloc]=
		cpudibp->backset[cpudibp->backalloc];
      }
      if(dci.copyfloats) {
          if(cpudibp->psr &  PSR_EF ) {  /* has the floating unit */
                clean_fp(cpudibp);
                cpudibp->psr &= ~PSR_EF;
          }
	  for(i=0;i<32;i++) {
             rn->pf.dib->fp_regs[i]=cpudibp->fp_regs[i];
          }
      }

/* none of the keys to be copied here can be involvedr */

      if(dci.copykeys) {
          mask=0x8000;
          for(i=0;i<16;i++) {
             if(mask & dci.keymask) {
                 if(cpudibp->keysnode->keys[i].type & involvedr) {
		    s=&dk0;  /* should not be possible */
                 }
                 else {
                    s = readkey(&(cpudibp->keysnode->keys[i]));
                    if(!s) s=&dk0;  /* should not be possible */
                 }
                 tempkey=*s;
                 tempkey.type &= ~involvedw;
                 s=&(rn->pf.dib->keysnode->keys[i]);
                 if(!clean(s)) set_slot(s,&tempkey);
             }
             mask = mask >> 1;
          }
      }
      if(dci.copyslot10) {  /* cant be involved */
          s = readkey(&cpudibp->rootnode->keys[10]);
          if(!s) s=&dk0;
          tempkey=*s;
          s = &(rn->keys[10]);
          if(!clean(s)) set_slot(s,&tempkey);
      }
      if(dci.copyslot11) {  /* cant be involved */
          s = readkey(&cpudibp->rootnode->keys[11]);
          if(!s) s=&dk0;
          tempkey=*s;
          s = &(rn->keys[11]);
          if(!clean(s)) set_slot(s,&tempkey);
      }
      if(dci.substitutedomain) {
          s=&(rn->pf.dib->keysnode->keys[dci.domainkeyslot]);
          tempkey.type = domainkey+prepared;
          tempkey.databyte=0;
          tempkey.nontypedata.ik.item.pk.subject=(union Item *)rn;
          if(!clean(s)) set_slot(s,&tempkey);
      }
      rn->pf.dib->pc=cpudibp->pc;
      rn->pf.dib->npc=cpudibp->npc;
      rn->pf.dib->psr=cpudibp->psr;
      rn->pf.dib->fsr=cpudibp->fsr;
      rn->pf.dib->permits |= (cpudibp->permits & GATEJUMPSPERMITTED);

      simplest(0);
      return;
     }
    default: simplest(KT+2);
      return;
   } /* End switch on order code */
} /* End jdom88kc */
